#pragma once
// levels.new_game(member): the next new game starts in `member` instead of exanima01. The fresh-level loader
// 0x181450(name16) is called by new game 0x126050 with "exanima01" (also by the 0x126eb0 load fallback, and by the
// arena paths 0x125a20/0x189f10 with other names). A one-shot flag armed from Lua swaps that one name; anything else
// passes untouched, and choosing any base title row (a plain BEGIN, CONTINUE, ...) disarms it, so a cancelled
// Endless start never changes an ordinary game. The game's own flow (character creation, spawn) is unchanged; an
// Endless title row reaches it with acts_as = 1 (BEGIN).
namespace newgame {
using Load=uintptr_t(*)(const char*);
static Load original=nullptr;static bool created=false,installed=false,possible=false;
static SRWLOCK lock=SRWLOCK_INIT;static char armed[17]{};
static bool install();
// The loader is hooked at the first arm, so games without Endless never carry this detour.
static bool arm(const std::string& member){
 if(member.empty()||member.size()>16||!possible||!install())return false;
 AcquireSRWLockExclusive(&lock);std::memset(armed,0,sizeof armed);std::memcpy(armed,member.data(),member.size());ReleaseSRWLockExclusive(&lock);return true;
}
static void disarm(){AcquireSRWLockExclusive(&lock);std::memset(armed,0,sizeof armed);ReleaseSRWLockExclusive(&lock);}
static void baseRowChosen(int){disarm();}
static uintptr_t load(const char* name){
 char swap[16]{};bool use=false;
 __try{
  if(name&&!std::memcmp(name,"exanima01\0\0\0\0\0\0\0",16)){
   AcquireSRWLockExclusive(&lock);if(armed[0]){std::memcpy(swap,armed,16);std::memset(armed,0,sizeof armed);use=true;}ReleaseSRWLockExclusive(&lock);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 return original(use?swap:name);
}
// push rbx; push rsi; lea rsp,[rsp-0x78]; mov rbx,rcx: ten bytes, four whole instructions, no RIP-relative operand.
static const unsigned char prefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0x88,0x48,0x89,0xcb};
static void available(){possible=gamebridge::operation(base+0x181450,prefix,sizeof prefix);}
static bool install(){
 if(installed)return true;
 if(!created){if(!gamebridge::operation(base+0x181450,prefix,sizeof prefix)||MH_CreateHook((void*)(base+0x181450),(void*)load,(void**)&original)!=MH_OK)return false;created=true;}
 const auto s=MH_EnableHook((void*)(base+0x181450));installed=s==MH_OK||s==MH_ERROR_ENABLED;
 if(installed)titlerows::onBaseRow=baseRowChosen;return installed;
}
}
