#pragma once
using NativePresent=void(*)();
using MenuDraw=void(*)(uintptr_t);
static NativePresent originalPresent=nullptr;static MenuDraw originalMenuDraw=nullptr;
static std::atomic<DWORD> titleDrawThread{0};static HWND presentationWindow=nullptr;
static bool titleObject(uintptr_t object){
 __try{return readable(base+0x4f0170,8)&&object==*(uintptr_t*)(base+0x4f0170)&&readable(object,0x220)&&*(int*)(object+0x208)==0;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// The one detour on the menu draw 0x128f30: it also carries the shared title rows (title_rows_bridge.h), which
// therefore never hook this function a second time.
static void observeMenu(uintptr_t object){
 // While Settings is open over the title menu, the menu's buttons are not drawn (they showed through the form).
 const bool hidden=runtimeEnabled&&titleObject(object)&&settingstabs::optionsOpen();
 if(hidden){}else if(titlerows::installed)titlerows::menuDraw(object);else originalMenuDraw(object);
 if(runtimeEnabled&&titleObject(object))titleDrawThread=GetCurrentThreadId();
}
static void presentationBody(HDC dc){
 if(!runtimeEnabled||!frameReady)return;
 // The renderer presents the HDC stored in this native slot (0x21e60).
 if(!readable(base+0x499610,8)||dc!=*(HDC*)(base+0x499610))return;
 const HWND window=WindowFromDC(dc);DWORD pid=0;const DWORD owner=window?GetWindowThreadProcessId(window,&pid):0;
 const DWORD current=GetCurrentThreadId();
 if(!window||!owner||owner!=current||pid!=GetCurrentProcessId()||GetAncestor(window,GA_ROOT)!=window||GetWindow(window,GW_OWNER)||!IsWindowVisible(window))return;
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);
 if(presentationWindow&&presentationWindow!=window)return;presentationWindow=window;
 if(!readable(base+0x4f0830,8))return;
 const auto player=*(uintptr_t*)(base+0x4f0830);const bool title=titleDrawThread.exchange(0)==current;
 pumpPresentation(current,owner,player?"session":title?"menu":"loading",player,GetTickCount64());
}
static void presentationSafe(HDC dc){try{presentationBody(dc);}catch(...){host.report("runtime","error","Lifecycle pulse fault; runtime disabled");runtimeEnabled=false;gamebridge::active=false;}}
static void present(){
 // The mod console and notices are drawn into the frame the game is about to present (console_bridge.h).
 if(runtimeEnabled&&readable(base+0x499610,8))console::draw(*(HDC*)(base+0x499610));
 originalPresent();
 __try{if(readable(base+0x499610,8))presentationSafe(*(HDC*)(base+0x499610));}
 __except(EXCEPTION_EXECUTE_HANDLER){runtimeEnabled=false;gamebridge::active=false;}
}
static bool installPresentation(){
 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)module,&pinned))return false;
 const auto target=(void*)(base+0x21e60);if(!hookLibrary())return false;
 // Whether the menu draw still had the game's own entry before this runtime took it: the shared title rows
 // build on this detour only then, so they never ride on another mod's menu hook.
 const unsigned char drawPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x50,0xff,0xff,0xff};
 titlerows::drawWasOriginal=titlerows::untouched(base+titlerows::rva::menuDraw,drawPrefix,sizeof drawPrefix);
 if(MH_CreateHook(target,(void*)present,(void**)&originalPresent)!=MH_OK)return false;
 if(MH_CreateHook((void*)(base+0x128f30),(void*)observeMenu,(void**)&originalMenuDraw)!=MH_OK){MH_RemoveHook(target);return false;}
 titlerows::originalMenuDraw=originalMenuDraw;
 if(MH_EnableHook(target)!=MH_OK)return false;
 if(MH_EnableHook((void*)(base+0x128f30))!=MH_OK){MH_DisableHook(target);return false;}
 return true;
}
