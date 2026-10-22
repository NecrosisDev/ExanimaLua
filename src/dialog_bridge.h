#pragma once
// ui.dialog(title, text): the game's own message box, the one the arena shows for "Insufficient funds":
// 306B0(title, text, 0x41, nullptr) (static reading). Flag 1 gives it an OK button
// that closes it, 0x40 the framed style; no callback. Title and text are Free Pascal constant strings (reference count
// -1, length before the bytes); the box keeps pointers to them, so they live in a static pool of 128 that is reused
// in turn (a slot comes back after 64 later boxes, long after its box closed). Built as a job on the game ring
// 0x499990 (it creates a GUI form); one box at a time, four waiting at most.
namespace dialogs {
using Show=void*(*)(const char*,const char*,uint32_t,void*);
struct Text {int64_t references=-1,length=0;char bytes[504]{};};
static std::array<Text,128> pool{};static size_t used=0;
static bool possible=false;
struct Job {std::atomic<bool> busy{false};const char* title=nullptr;const char* text=nullptr;};
static Job job;static std::mutex lock;static std::vector<std::pair<std::string,std::string>> wanted;
static void available(){
 const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xb8,0x48,0x89,0x7d};
 possible=gamebridge::operation(base+0x306b0,prefix,sizeof prefix)&&readable(base+0x49a930,8);
}
// Code page 1252, as the game's own strings; at most 480 bytes of text and 60 of title.
static const char* keep(const std::string& utf8,size_t limit){
 Text& t=pool[used++%pool.size()];std::wstring w(utf8.size(),L'\0');
 const int n=MultiByteToWideChar(CP_UTF8,0,utf8.data(),int(utf8.size()),w.data(),int(w.size()));w.resize(size_t((std::max)(n,0)));
 int m=WideCharToMultiByte(1252,0,w.data(),int(w.size()),t.bytes,int((std::min)(limit,sizeof t.bytes-1)),"?",nullptr);if(m<0)m=0;
 t.bytes[m]=0;t.length=m;return t.bytes;
}
static bool request(const std::string& title,const std::string& text){
 if(!possible)return false;std::lock_guard<std::mutex> l(lock);if(wanted.size()>=4)return false;wanted.push_back({title,text});return true;
}
static unsigned char run(void*){
 __try{uintptr_t desktop=0;if(copyWorldMemory(base+0x49a930,&desktop,8)&&desktop)((Show)(base+0x306b0))(job.title,job.text,0x41,nullptr);}__except(EXCEPTION_EXECUTE_HANDLER){}
 job.busy=false;return 0;
}
static void deliver(){
 if(!possible||job.busy.load())return;std::pair<std::string,std::string> next;
 {std::lock_guard<std::mutex> l(lock);if(wanted.empty())return;next=wanted.front();wanted.erase(wanted.begin());}
 if(job.busy.exchange(true))return;
 job.title=keep(next.first,60);job.text=keep(next.second,480);if(!nativecalls::enqueue(run))job.busy=false;
}
}
