#pragma once
// The game's own error and diagnostic messages. Every report goes through the function pointer slot 0x2ab6b0
// (level, caption, text); startup points it at the logger 0x25d80, whose levels 4 and 5 show a message box. That
// includes the dialogue and role script errors ("Script Error", "Invalid role identifier.", "Process Event"), which
// otherwise reach no player. The runtime chains the slot once at the settled title menu: the replacement copies the
// two strings into a bounded queue and then calls the previous function with the same arguments, so the game's
// own logging and boxes are unchanged. Messages reach ScriptRuntime.log and game.message handlers at the next pulse.
namespace gamemessages {
using Sink=uint32_t(*)(uint32_t,const char*,const char*);
struct Message {uint32_t level=0;char caption[64]{};char text[256]{};};
static std::atomic<Sink> previous{nullptr};static bool installed=false;
static SRWLOCK lock=SRWLOCK_INIT;
static std::array<Message,32> queue;static size_t queued=0,lost=0;
static std::map<std::string,unsigned> seen;   // how often each distinct message reached the log
static constexpr unsigned logRepeats=3;static constexpr size_t seenMax=512;
// Up to N-1 bytes of a C string the game passed; a fault ends the copy where it happened.
static void copyText(const char* text,char* out,size_t capacity){
 size_t n=0;
 __try{if(text)for(;n+1<capacity;++n){const char c=text[n];if(!c)break;out[n]=(c=='\r'||c=='\n'||c=='\t')?' ':c;}}__except(EXCEPTION_EXECUTE_HANDLER){}
 out[n]=0;
}
static uint32_t sink(uint32_t level,const char* caption,const char* text){
 Message m;m.level=level;copyText(caption,m.caption,sizeof m.caption);copyText(text,m.text,sizeof m.text);
 AcquireSRWLockExclusive(&lock);if(queued<queue.size())queue[queued++]=m;else ++lost;ReleaseSRWLockExclusive(&lock);
 const auto next=previous.load();return next?next(level,caption,text):level;
}
// The slot must hold the shipped logger or the release stub (0x187f0: mov eax,ecx; ret). Anything else belongs to
// another mod, which is left in place. EMTK remaps the image at startup, which leaves .data execute-read-write, so
// any writable protection is accepted. On failure `why` says what was found.
static std::string why;
static bool install(){
 if(installed)return true;
 const unsigned char logger[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x89,0xcb};const unsigned char stub[]={0x89,0xc8,0xc3};
 const auto slot=base+0x2ab6b0;uintptr_t current=0;MEMORY_BASIC_INFORMATION m{};
 if(!copyWorldMemory(slot,&current,8)||VirtualQuery((void*)slot,&m,sizeof m)!=sizeof m){why="the message slot could not be read";return false;}
 if(!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){char b[64];snprintf(b,sizeof b,"the message slot is not writable (protection 0x%lx)",m.Protect);why=b;return false;}
 if(!((current==base+0x25d80&&gamebridge::operation(current,logger,sizeof logger))||(current==base+0x187f0&&gamebridge::operation(current,stub,sizeof stub)))){
  char b[96];if(current>=base&&current<base+0x4000000)snprintf(b,sizeof b,"the message slot points at game code +0x%llx, not the logger",(unsigned long long)(current-base));
  else snprintf(b,sizeof b,"the message slot points outside the game (another mod's handler)");why=b;return false;}
 previous=(Sink)current;
 // Pointer-sized aligned exchange; it fails, and nothing changes, if the slot moved since it was read.
 installed=InterlockedCompareExchangePointer((PVOID*)slot,(PVOID)&sink,(PVOID)current)==(PVOID)current;
 if(!installed)previous=nullptr;return installed;
}
static void clear(){AcquireSRWLockExclusive(&lock);queued=lost=0;ReleaseSRWLockExclusive(&lock);}
static bool replacedReported=false;static uint64_t droppedTotal=0;
static void deliver(scripting::Host& owner){
 // Someone rewriting the slot after install silences this feature; say so once.
 uintptr_t current=0;
 if(installed&&!replacedReported&&copyWorldMemory(base+0x2ab6b0,&current,8)&&current!=(uintptr_t)&sink){replacedReported=true;owner.report("runtime","warn","Game messages stopped: the game's message slot was replaced after the runtime chained it");}
 std::vector<Message> messages;size_t missed=0;
 AcquireSRWLockExclusive(&lock);messages.assign(queue.begin(),queue.begin()+ptrdiff_t(queued));missed=lost;queued=lost=0;ReleaseSRWLockExclusive(&lock);
 // Dropped messages are reported the first time and then every 1000, so a chatty logger cannot flood the log.
 if(missed){const auto before=droppedTotal;droppedTotal+=missed;if(!before||before/1000!=droppedTotal/1000)
  owner.report("game","warn",("Game messages dropped (more than 32 in one pulse): "+std::to_string(droppedTotal)+" so far").c_str());}
 std::vector<scripting::GameMessage> events;
 for(const auto& m:messages){
  const std::string line=std::string(m.caption)+": "+m.text;
  if(seen.size()>=seenMax&&!seen.count(line))seen.clear();const unsigned count=++seen[line];
  // Levels 4 and 5 are the ones the game shows in a message box; lower levels are its log notes.
  if(count<=logRepeats)owner.report("game",m.level>=4?"error":"info",count==logRepeats?(line+" (repeats are no longer logged)").c_str():line.c_str());
  if(owner.handlesMessages())events.push_back({m.caption,m.text,int(m.level)});
 }
 if(!events.empty())owner.notifyMessages(events);
}
}
