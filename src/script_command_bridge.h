#pragma once
#include <cctype>
// Global commands for the game's role, dialogue and zone scripts, added with the game's own registrar 0x1c38f0.
// Lookup 0x1c3a60 scans the table from the first entry by name each call and stores the result at vm +0x10,
// so added names cannot shadow native ones. A command runs on the game thread inside a script: it reads its
// raw argument tokens (count at vm +0x50, {text, length} pairs from +0x58) and never calls Lua.
//   LuaEvent(name[, value])  queues an event that addons receive at the next session pulse; returns 1.
//   LuaFlag(name)            returns the integer an addon set with script_flags.set, else 0.
//   LuaLog(words...)         writes its arguments to ScriptRuntime.log, so script authors can trace a script.
// The game reports nothing for a bad call, so these log what they refused (each distinct problem a few times).
namespace scriptcommands {
using Register=void(*)(const char*,void*);using Number=int32_t(*)(uintptr_t,int);
// The registrar keeps the pointer and reads the length 8 bytes before it, as for the game's own strings;
// these live as long as the pinned DLL and are marked constant (reference count -1).
struct Name {uint16_t codePage=0,elementSize=1;uint32_t pad=0;int64_t references=-1,length=0;char text[16]{};};
static Name eventName{0,1,0,-1,8,"LuaEvent"},flagName{0,1,0,-1,7,"LuaFlag"},logName{0,1,0,-1,6,"LuaLog"};
static bool registered=false,logRegistered=false;
// The game's own argument reader 0x1c4090(vm, index): a decimal literal, or the value of a script variable such as
// ZoneID (0 when absent). It has no side effects.
static Number number=nullptr;
static std::atomic<bool> listening{false};
static SRWLOCK lock=SRWLOCK_INIT;
struct Event {char name[33]{};char value[33]{};bool numberKnown=false;int32_t number=0;};
static std::array<Event,32> queue;static size_t queued=0,lost=0;
// Script-side notes, reported from the pulse rather than from the script's thread: LuaLog lines (at most 60 a
// minute), refused calls (warn, the first three times each exact text is seen) and hints (info, once each).
enum class NoteKind : uint8_t {line,problem,hint};
struct Note {NoteKind kind=NoteKind::line;char text[200]{};};
static std::array<Note,32> notes;static size_t noted=0;
static std::map<std::string,unsigned> problems;static constexpr unsigned problemRepeats=3;static constexpr size_t problemsMax=512;
static constexpr unsigned linesPerMinute=60;static uint64_t lineMinute=0;static unsigned linesThisMinute=0,linesHeld=0;
static void note(NoteKind kind,const char* text){
 AcquireSRWLockExclusive(&lock);
 if(noted<notes.size()){auto& n=notes[noted++];n.kind=kind;strncpy_s(n.text,text,_TRUNCATE);}
 ReleaseSRWLockExclusive(&lock);
}
static std::map<std::string,int32_t> flags;
static constexpr size_t flagsMax=256;
static int count(uintptr_t vm){__try{const int n=*(const int*)(vm+0x50);return n<0||n>64?-1:n;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}}
// The raw token as written (at most `capacity`-1 bytes, control characters shown as '?'), for messages.
static void token(uintptr_t vm,int index,char* out,size_t capacity){
 size_t n=0;
 __try{
  if(index<count(vm)){const auto slot=vm+0x58+uintptr_t(index)*0x10;const char* text=*(const char* const*)slot;uint32_t size=*(const uint32_t*)(slot+8);
   if(text)for(;n<size&&n+1<capacity;++n){const char c=text[n];out[n]=(c<0x20||c==0x7f)?'?':c;}}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 out[n]=0;
}
// Copies argument `index` (at most 32 characters, trailing spaces dropped: the tokenizer keeps them); false
// when the script passed fewer arguments.
static bool argument(uintptr_t vm,int index,char (&out)[33]){
 __try{
  const int count=*(const int*)(vm+0x50);if(index>=count||count>64)return false;
  const auto slot=vm+0x58+uintptr_t(index)*0x10;const char* text=*(const char* const*)slot;uint32_t size=*(const uint32_t*)(slot+8);
  if(!text||size>64)return false;
  while(size&&text[size-1]==' ')--size;if(size>32)return false;
  for(uint32_t i=0;i<size;++i)out[i]=text[i];out[size]=0;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Letters, digits and '_'. The game's tokenizer reads past ')' for an empty call such as LuaEvent(), so a
// name made of anything else is a stray piece of the script and is refused.
static bool plainName(const char* name){
 if(!*name)return false;for(;*name;++name)if(!(std::isalnum((unsigned char)*name)||*name=='_'))return false;return true;
}
static void problem(const char* command,uintptr_t vm,const char* why,NoteKind kind=NoteKind::problem){
 char raw[48]{};token(vm,0,raw,sizeof raw);char line[200];
 snprintf(line,sizeof line,"%s(%s): %s",command,raw,why);note(kind,line);
}
static bool evaluate(uintptr_t vm,int index,int32_t& out){__try{if(!number||index>=count(vm))return false;out=number(vm,index);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static int luaEvent(uintptr_t vm){
 if(!listening.load(std::memory_order_relaxed)||!runtimeEnabled.load())return 0;
 Event e;if(!argument(vm,0,e.name)||!plainName(e.name)){problem("LuaEvent",vm,"the first argument must be a name of 1..32 letters, digits or _ (event not sent)");return 0;}
 if(count(vm)>1){argument(vm,1,e.value);e.numberKnown=evaluate(vm,1,e.number);}
 AcquireSRWLockExclusive(&lock);if(queued<queue.size())queue[queued++]=e;else ++lost;ReleaseSRWLockExclusive(&lock);
 return 1;
}
static int luaFlag(uintptr_t vm){
 if(!runtimeEnabled.load())return 0;
 char name[33]{};if(!argument(vm,0,name)||!plainName(name)){problem("LuaFlag",vm,"the argument must be a name of 1..32 letters, digits or _ (reads 0)");return 0;}
 int32_t value=0;bool found=false;
 try{const std::string key(name);AcquireSRWLockShared(&lock);const auto at=flags.find(key);found=at!=flags.end();if(found)value=at->second;ReleaseSRWLockShared(&lock);}catch(...){}
 if(!found)problem("LuaFlag",vm,"read before any addon set it, so it reads 0 (logged once; check the spelling if an addon should have set it)",NoteKind::hint);
 return value;
}
// Every argument as written, space separated; returns 1.
static int luaLog(uintptr_t vm){
 if(!runtimeEnabled.load())return 0;
 char line[200]{};size_t used=0;const int n=count(vm);
 for(int i=0;i<n&&used+2<sizeof line;++i){if(i)line[used++]=' ';token(vm,i,line+used,sizeof line-used);used+=strnlen(line+used,sizeof line-used);}
 note(NoteKind::line,line);return 1;
}
static bool validName(const std::string& name){
 if(name.empty()||name.size()>32)return false;
 for(unsigned char c:name)if(!(std::isalnum(c)||c=='_'))return false;return true;
}
static bool setFlag(const std::string& name,const int32_t* value){
 if(!validName(name))return false;bool ok=true;
 AcquireSRWLockExclusive(&lock);
 if(!value)flags.erase(name);else if(flags.count(name)||flags.size()<flagsMax)flags[name]=*value;else ok=false;
 ReleaseSRWLockExclusive(&lock);return ok;
}
static bool getFlag(const std::string& name,int32_t& value){
 AcquireSRWLockShared(&lock);const auto found=flags.find(name);const bool ok=found!=flags.end();if(ok)value=found->second;ReleaseSRWLockShared(&lock);return ok;
}
static void clear(){AcquireSRWLockExclusive(&lock);flags.clear();queued=lost=noted=0;ReleaseSRWLockExclusive(&lock);}
static bool present(uintptr_t table,int count,const Name& name){
 __try{
  for(int i=0;i<count;++i){const auto row=table+uintptr_t(i)*0x18;
   if(*(const int*)(row+8)==int(name.length)&&!std::memcmp(*(const char* const*)row,name.text,size_t(name.length)))return true;}
  return false;
 }__except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
static bool registerCall(Register at,const Name& name,int(*command)(uintptr_t)){__try{at(name.text,(void*)command);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool eventRegistered=false,flagRegistered=false;
// Runs once at the settled title menu, where no level or dialogue is active to run a role script while the
// registrar grows the table (inferred; not shown which thread runs scripts). LuaLog is optional: the other two
// stay available when its name is taken.
static bool install(){
 if(registered)return true;
 const unsigned char prefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6,0x83,0x05,0xdc,0x67,0x3a,0x00,0x01};
 if(!gamebridge::operation(base+0x1c38f0,prefix,sizeof prefix))return false;
 // Fourteen bytes, four whole instructions, no RIP-relative operand; a mismatch only drops event.number.
 const unsigned char numberPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x3b,0x53,0x50};
 if(!number&&gamebridge::operation(base+0x1c4090,numberPrefix,sizeof numberPrefix))number=(Number)(base+0x1c4090);
 int count=0;uintptr_t table=0;
 if(!copyWorldMemory(base+0x56a0e0,&count,4)||!copyWorldMemory(base+0x56a0f0,&table,8)||count<1||count>4096||!table)return false;
 // A name someone else registered would make ours unreachable; one this runtime registered before a
 // failure is kept and only the missing one is added.
 if((!eventRegistered&&present(table,count,eventName))||(!flagRegistered&&present(table,count,flagName)))return false;
 const auto at=(Register)(base+0x1c38f0);
 if(!eventRegistered)eventRegistered=registerCall(at,eventName,luaEvent);
 if(eventRegistered&&!flagRegistered)flagRegistered=registerCall(at,flagName,luaFlag);
 registered=eventRegistered&&flagRegistered;
 // The registrar may have grown (moved) the table: read it again before looking for LuaLog.
 if(registered&&!logRegistered&&copyWorldMemory(base+0x56a0e0,&count,4)&&copyWorldMemory(base+0x56a0f0,&table,8)&&count>0&&count<=4096&&table&&
    !present(table,count,logName))logRegistered=registerCall(at,logName,luaLog);
 return registered;
}
static void deliver(scripting::Host& owner,bool inSession){
 listening=registered&&owner.handlesScriptEvents();
 std::vector<scripting::ScriptEvent> events;std::vector<Note> pending;size_t missed=0;
 AcquireSRWLockExclusive(&lock);for(size_t i=0;i<queued;++i)events.push_back({queue[i].name,queue[i].value,queue[i].numberKnown,queue[i].number});
 pending.assign(notes.begin(),notes.begin()+ptrdiff_t(noted));missed=lost;queued=lost=noted=0;ReleaseSRWLockExclusive(&lock);
 const uint64_t minute=GetTickCount64()/60000;
 if(minute!=lineMinute){
  if(linesHeld)owner.report("script","warn",(std::to_string(linesHeld)+" LuaLog lines were not written (more than 60 in a minute)").c_str());
  lineMinute=minute;linesThisMinute=linesHeld=0;
 }
 for(const auto& n:pending){
  if(n.kind==NoteKind::line){if(linesThisMinute<linesPerMinute){++linesThisMinute;owner.report("script","info",n.text);}else ++linesHeld;continue;}
  if(problems.size()>=problemsMax&&!problems.count(n.text))problems.clear();
  const unsigned seen=++problems[n.text],limit=n.kind==NoteKind::hint?1:problemRepeats;
  if(seen<=limit)owner.report("script",n.kind==NoteKind::hint?"info":"warn",seen==limit&&limit>1?(std::string(n.text)+" (repeats are no longer logged)").c_str():n.text);
 }
 if(!listening||!inSession)return;
 if(missed)owner.report("runtime","warn","Script events dropped: more than 32 in one pulse");
 if(!events.empty())owner.notifyScriptEvents(events);
}
}
