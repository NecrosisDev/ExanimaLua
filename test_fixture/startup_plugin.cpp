#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstring>
static unsigned calls=0;
static unsigned scripts=0;
extern "C" __declspec(dllexport) bool enable(){
 ++calls;
#ifdef DECLINE_ENABLE
 return false;
#else
 return true;
#endif
}
#ifndef MISSING_DISABLE
extern "C" __declspec(dllexport) bool disable(){return true;}
#endif
extern "C" __declspec(dllexport) unsigned enable_count(){return calls;}
extern "C" __declspec(dllexport) bool load_script_addon(const char* id,const unsigned char* source,size_t size,const char* caps){
 if(!id||!source||!size||!caps)return false;
 ++scripts;return true;
}
extern "C" __declspec(dllexport) unsigned script_count(){return scripts;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
