#include "plugin.cpp"
#include <cassert>
#include <cstdio>
#include "test_assert.h"

using Call=int(*)();
static Call chained=nullptr;
static int replacement(){return chained()+100;}
static void absoluteJump(unsigned char* at,uintptr_t target){
 at[0]=0xff;at[1]=0x25;std::memset(at+2,0,4);std::memcpy(at+6,&target,8);
}
int main(){
 auto page=(unsigned char*)VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 assert(page);std::memset(page,0x90,4096);
 const unsigned char prefix[]={0xb8,7,0,0,0,0xc3};
 std::memcpy(page,prefix,sizeof prefix);
 assert(gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 page[1]=8;assert(!gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 // An existing mod's FF25 detour is accepted; our hook must preserve its result.
 const unsigned char prior[]={0xb8,17,0,0,0,0xc3};
 std::memcpy(page+64,prior,sizeof prior);
 absoluteJump(page,(uintptr_t)(page+64));
 FlushInstructionCache(GetCurrentProcess(),page,4096);
 assert(gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 auto call=(Call)page;assert(call()==17);
 assert(MH_Initialize()==MH_OK);
 assert(MH_CreateHook(page,(void*)replacement,(void**)&chained)==MH_OK);
 assert(MH_EnableHook(page)==MH_OK);assert(call()==117);
 assert(MH_DisableHook(page)==MH_OK);assert(call()==17);
 assert(MH_Uninitialize()==MH_OK);
 // The production presentation callback must preserve an existing provider's result.
 absoluteJump(page,(uintptr_t)(page+64));FlushInstructionCache(GetCurrentProcess(),page,4096);
 assert(MH_Initialize()==MH_OK);assert(MH_CreateHook(page,(void*)present,(void**)&originalPresent)==MH_OK);
 assert(MH_EnableHook(page)==MH_OK);runtimeEnabled=true;frameReady=true;
 ((NativePresent)page)();runtimeEnabled=false;frameReady=false;
 assert(MH_DisableHook(page)==MH_OK&&((Call)page)()==17);assert(MH_Uninitialize()==MH_OK);
 // Do not accept broken or cyclic entry detours merely because they are jumps.
 absoluteJump(page,0);assert(!gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 absoluteJump(page,(uintptr_t)(page+64));absoluteJump(page+64,(uintptr_t)page);
 assert(!gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 absoluteJump(page,(uintptr_t)(page+64));std::memcpy(page+64,prior,sizeof prior);
 DWORD old=0;assert(VirtualProtect(page,4096,PAGE_READWRITE,&old));
 assert(!gamebridge::operation((uintptr_t)page,prefix,sizeof prefix));
 VirtualFree(page,0,MEM_RELEASE);
 std::puts("LUA_HOOK_COMPATIBILITY_PASSED: operation detection, existing detour chaining, disable restores prior mod, invalid/cyclic/non-executable rejection");
}
