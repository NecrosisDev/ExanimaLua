#include "plugin.cpp"
#include "test_assert.h"
#include <cstdio>
static unsigned nativeCalls=0;
static BOOL WINAPI nativeSwap(HDC){++nativeCalls;return 17;}
int main(int argc,char** argv){
 assert(argc==2);const bool cameraFirst=!std::strcmp(argv[1],"camera-first");
 auto camera=LoadLibraryW(L"camera-hooks.dll");assert(camera);
 auto install=(int(*)(void*))GetProcAddress(camera,"cameraInstall");
 auto stop=(int(*)())GetProcAddress(camera,"cameraStop");
 auto rearm=(int(*)())GetProcAddress(camera,"cameraRearm");
 auto finish=(int(*)())GetProcAddress(camera,"cameraFinish");
 auto calls=(unsigned(*)())GetProcAddress(camera,"cameraCalls");
 assert(install&&stop&&rearm&&finish&&calls);
 auto image=(unsigned char*)VirtualAlloc(nullptr,0x500000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(image);
 base=(uintptr_t)image;
 // Exact archived void presentation wrapper, including both renderer branches.
 const unsigned char wrapper[]={0x48,0x8d,0x64,0x24,0xd8,0x48,0x83,0x3d,0x93,0x6a,0x47,0,0,0x74,0x0f,0x48,0x8b,0x0d,0x9a,0x77,0x47,0,0xff,0x15,0x84,0x6a,0x47,0,0xeb,0x0d,0x48,0x8b,0x0d,0x8b,0x77,0x47,0,0xff,0x15,0x25,0x6a,0x47,0,0x90,0x48,0x8d,0x64,0x24,0x28,0xc3};
 std::memcpy(image+0x21e60,wrapper,sizeof wrapper);
 // A separate OpenGL entry, owned by ExCamera's actual hook manager.
 auto endpoint=image+0x100;endpoint[0]=0x48;endpoint[1]=0xb8;
 const auto target=(uintptr_t)nativeSwap;std::memcpy(endpoint+2,&target,8);endpoint[10]=0xff;endpoint[11]=0xe0;
 *(void**)(image+0x498900)=endpoint;*(void**)(image+0x4988b0)=endpoint;
 FlushInstructionCache(GetCurrentProcess(),image,0x500000);
 if(cameraFirst)assert(install(endpoint)==0);
 assert(MH_Initialize()==MH_OK);
 assert(MH_CreateHook(image+0x21e60,(void*)present,(void**)&originalPresent)==MH_OK);
 assert(MH_EnableHook(image+0x21e60)==MH_OK);
 if(!cameraFirst)assert(install(endpoint)==0);
 auto draw=(NativePresent)(image+0x21e60);
 runtimeEnabled=true;frameReady=true;draw();assert(nativeCalls==1&&calls()==1);
 assert(((BOOL(WINAPI*)(HDC))endpoint)(nullptr)==17);assert(nativeCalls==2&&calls()==2);
 assert(stop()==0);draw();assert(nativeCalls==3&&calls()==2);
 assert(rearm()==0);draw();assert(nativeCalls==4&&calls()==3);
 // Native fallback branch still traverses the current camera chain.
 *(void**)(image+0x498900)=nullptr;draw();assert(nativeCalls==5&&calls()==4);
 assert(disable());draw();assert(nativeCalls==6&&calls()==5);
 // Camera can retire its own hooks while the inert Lua wrapper stays installed.
 assert(stop()==0&&finish()==0);draw();assert(nativeCalls==7&&calls()==5);
 assert(MH_DisableHook(image+0x21e60)==MH_OK);assert(MH_Uninitialize()==MH_OK);
 draw();assert(nativeCalls==8);VirtualFree(image,0,MEM_RELEASE);FreeLibrary(camera);
 std::printf("LUA_EXCAMERA_COEXISTENCE_PASSED: %s, native branches, forwarding, camera stop/rearm, Lua disable, independent teardown\n",argv[1]);
}
