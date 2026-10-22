// Built with the specified ExCamera candidate's unmodified MinHook sources.
#include <windows.h>
#include "vendor/minhook/include/MinHook.h"
using Swap=BOOL(WINAPI*)(HDC);
static Swap previous=nullptr;
static unsigned calls=0;
static BOOL WINAPI cameraSwap(HDC dc){++calls;return previous(dc);}
extern "C" __declspec(dllexport) int cameraInstall(void* target){
 if(MH_Initialize()!=MH_OK)return -1;
 if(MH_CreateHook(target,(void*)cameraSwap,(void**)&previous)!=MH_OK)return -2;
 return int(MH_EnableHook(MH_ALL_HOOKS));
}
extern "C" __declspec(dllexport) int cameraStop(){return int(MH_DisableHook(MH_ALL_HOOKS));}
extern "C" __declspec(dllexport) int cameraRearm(){return int(MH_EnableHook(MH_ALL_HOOKS));}
extern "C" __declspec(dllexport) int cameraFinish(){return int(MH_Uninitialize());}
extern "C" __declspec(dllexport) unsigned cameraCalls(){return calls;}
