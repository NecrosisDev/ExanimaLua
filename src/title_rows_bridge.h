#pragma once
// Shared title-menu row owner (exports add_title_row & co.). Extra rows go on the game's own title page (form
// 0x4f0170, page +0x208 == 0) by the method of the editor's reviewed EDITOR row,
// generalised to N rows by the Endless prototype, whose arithmetic this keeps. The six inline 0x18-byte rows (+0x168, count +0x160, text at row
// +0x10) hold the first six rows of the final order; the rest are drawn and hit-tested by the game's own code as a
// short extra menu placed where row 7 lies, then everything is put back.
// Five targets: build 0x122c40, hit 0x128d80, nav 0x128a70, activate 0x128580, and draw 0x128f30, which the
// runtime's own presentation detour already owns and serves (presentation.h observeMenu). Available only when every
// target had its original entry bytes (draw: before the runtime took it); the four hooks are made at the first
// registration, so a game without registered rows carries none of them.
namespace titlerows {
namespace rva {
inline constexpr uintptr_t menuBuild=0x122c40,menuDraw=0x128f30,menuHit=0x128d80,menuNav=0x128a70,menuActivate=0x128580,
 measure=0x128450,hoverTo=0x128c90,assign=0x80f0,width=0x276a0,menuObject=0x4f0170,guiDesktop=0x49a930;
}
// Six extra rows at most: the overflow is drawn through the six inline row records (0x18 bytes each from +0x168);
// a seventh would overwrite the menu's own fields from +0x1f8.
constexpr int baseRows=6,maxExtra=6,maxRows=baseRows+maxExtra;
// The game's own string literal layout (code page, element size, pad, reference count -1, length, text).
struct alignas(8) Ansi {uint16_t cp=0,elem=1;uint32_t pad=0;int64_t ref=-1,len=0;char text[32]{};};
using Action=void(*)(void*);
// `retired`: unregistered, but its text may still be on screen until the next draw rebuilds the menu, so the
// slot is not reused before then.
// `actsAs`: after its action, the row does what base row 1..6 does (0: nothing more), for a mode that starts
// through the game's own flow (Endless: BEGIN's character creation).
struct Extra {Ansi label;int afterBase=0,actsAs=0;Action action=nullptr;void* context=nullptr;bool used=false,enabled=true,retired=false;};
using BaseRowHook=void(*)(int);static BaseRowHook onBaseRow=nullptr;   // told when a base row is chosen
static std::atomic<bool> dirty{false},ready{false},installed{false},drawWasOriginal{false};
static Extra extras[maxExtra];
static SRWLOCK extrasLock=SRWLOCK_INIT;
static std::atomic<int> activating{0};static thread_local bool insideActivate=false;
static bool installHooks();
static bool validLabel(const char* label){
 if(!label)return false;const size_t n=strnlen(label,32);if(!n||n>31)return false;
 for(size_t i=0;i<n;++i)if(label[i]<0x20||label[i]>0x7e)return false;return true;
}
// A row placed after base row `afterBase` (1 BEGIN .. 6 EXIT). The same action may register its label again (to
// move it or change its context); a label held by a different action is refused.
static bool registerRow(const char* label,int afterBase,Action action,void* context,int actsAs=0){
 if(!validLabel(label)||afterBase<1||afterBase>baseRows||actsAs<0||actsAs>baseRows||!action||!ready||!installHooks())return false;
 AcquireSRWLockExclusive(&extrasLock);bool ok=false,taken=false;
 for(Extra& e:extras)if(e.used&&!std::strcmp(e.label.text,label)){if(e.action==action){e.context=context;e.afterBase=afterBase;e.actsAs=actsAs;ok=true;}else taken=true;break;}
 if(!ok&&!taken)for(Extra& e:extras){if(e.used||e.retired)continue;
  std::memset(e.label.text,0,sizeof e.label.text);strncpy_s(e.label.text,sizeof e.label.text,label,_TRUNCATE);
  e.label.len=int64_t(std::strlen(e.label.text));e.afterBase=afterBase;e.actsAs=actsAs;e.action=action;e.context=context;e.used=true;e.enabled=true;ok=true;break;}
 ReleaseSRWLockExclusive(&extrasLock);if(ok)dirty=true;return ok;
}
static bool setRowEnabled(const char* label,bool enabled){
 if(!validLabel(label))return false;AcquireSRWLockExclusive(&extrasLock);bool ok=false;
 for(Extra& e:extras)if(e.used&&!std::strcmp(e.label.text,label)){e.enabled=enabled;ok=true;}
 ReleaseSRWLockExclusive(&extrasLock);if(ok)dirty=true;return ok;
}
// Returns after the row's action can no longer run; called from inside an activate it returns at once.
static bool unregisterRow(const char* label){
 if(!validLabel(label))return false;AcquireSRWLockExclusive(&extrasLock);bool ok=false;
 for(Extra& e:extras)if(e.used&&!std::strcmp(e.label.text,label)){e.used=false;e.retired=true;e.action=nullptr;e.context=nullptr;ok=true;}
 ReleaseSRWLockExclusive(&extrasLock);if(ok)dirty=true;
 if(ok&&!insideActivate)while(activating.load())Sleep(0);
 return ok;
}
// Whether `label` is held by `action` (the Lua rows keep their own; other mods may not remove them by label).
static bool heldBy(const char* label,Action action){
 if(!validLabel(label))return false;AcquireSRWLockShared(&extrasLock);bool held=false;
 for(const Extra& e:extras)if(e.used&&!std::strcmp(e.label.text,label))held=e.action==action;
 ReleaseSRWLockShared(&extrasLock);return held;
}
// title.set_subtitle: a short second line drawn small under a row (parallel to `extras`, under extrasLock; "" none).
struct alignas(8) AnsiLine {uint16_t cp=0,elem=1;uint32_t pad=0;int64_t ref=-1,len=0;char text[64]{};};   // a constant game AnsiString
static AnsiLine subtitles[maxExtra];
static bool setSubtitle(const char* label,const char* text){
 if(!validLabel(label)||!text)return false;AcquireSRWLockExclusive(&extrasLock);bool ok=false;
 for(size_t i=0;i<maxExtra;++i)if(extras[i].used&&!std::strcmp(extras[i].label.text,label)){
  auto& s=subtitles[i];std::memset(s.text,0,sizeof s.text);strncpy_s(s.text,sizeof s.text,text,_TRUNCATE);s.len=int64_t(std::strlen(s.text));ok=true;}
 ReleaseSRWLockExclusive(&extrasLock);return ok;
}
// Called when the menu is rebuilt (no row text on screen any more): retired slots can be reused.
static void releaseRetired(){AcquireSRWLockExclusive(&extrasLock);for(Extra& e:extras)e.retired=false;ReleaseSRWLockExclusive(&extrasLock);}

struct RowRecord {int width;uint32_t flags;int style;uintptr_t text;};
static_assert(sizeof(RowRecord)==0x18);
struct Armed {
 bool on=false;uintptr_t obj=0;int count=0;
 RowRecord original[baseRows];RowRecord final[maxRows];int extraIndex[maxRows];int baseIndex[maxRows];
};
static Armed armed;
static bool titleMenu(uintptr_t obj){return obj&&readable(base+rva::menuObject,8)&&obj==*(uintptr_t*)(base+rva::menuObject)&&readable(obj,0x220)&&*(int*)(obj+0x208)==0;}
static int textWidth(uintptr_t s){return ((int(*)(uintptr_t))(base+rva::width))(s);}
static bool rowIs(uintptr_t s,const char* text){const size_t n=std::strlen(text)+1;return readable(s,n)&&!std::memcmp((const void*)s,text,n);}
static RowRecord* rows(uintptr_t obj){return (RowRecord*)(obj+0x168);}
// Only the title as the game builds it (BEGIN, CONTINUE, ARENA, MANUAL, SETTINGS, EXIT); anything else is left alone.
static void arm(uintptr_t obj){
 if(*(int*)(obj+0x160)!=baseRows)return;
 static const char* const names[baseRows]={"BEGIN","CONTINUE","ARENA","MANUAL","SETTINGS","EXIT"};
 for(int i=0;i<baseRows;++i)if(!rowIs(rows(obj)[i].text,names[i]))return;
 AcquireSRWLockShared(&extrasLock);int count=0;Armed a;
 for(int b=1;b<=baseRows;++b){
  a.final[count]=rows(obj)[b-1];a.extraIndex[count]=-1;a.baseIndex[count]=b;++count;
  for(int e=0;e<maxExtra;++e)if(extras[e].used&&extras[e].enabled&&extras[e].afterBase==b&&count<maxRows){
   a.final[count]={textWidth((uintptr_t)extras[e].label.text),0,0,(uintptr_t)extras[e].label.text};a.extraIndex[count]=e;a.baseIndex[count]=0;++count;}
 }
 ReleaseSRWLockShared(&extrasLock);
 if(count==baseRows)return;
 std::memcpy(a.original,rows(obj),sizeof a.original);
 const auto assign=(void(*)(uintptr_t,uintptr_t))(base+rva::assign);   // the game's own string assignment
 for(int i=0;i<baseRows;++i)assign(obj+0x168+0x18*i+0x10,a.final[i].text);
 ((void(*)(uintptr_t))(base+rva::measure))(obj);   // row widths, as the builder does
 a.on=true;a.obj=obj;a.count=count;armed=a;
}
static void disarm(){
 if(!armed.on)return;const uintptr_t obj=armed.obj;
 __try{
  if(titleMenu(obj)&&rows(obj)[0].text==armed.final[0].text){
   *(int*)(obj+0x70)=0;*(int*)(obj+0x204)=0;if(*(int*)(obj+0x6c)>3)*(int*)(obj+0x6c)=0;
   for(int i=0;i<baseRows;++i){rows(obj)[i].text=armed.original[i].text;rows(obj)[i].width=textWidth(armed.original[i].text);}
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 armed.on=false;
}
struct Saved {RowRecord row[maxExtra];int count,hover,pressed;float start;unsigned char logo;};
// The overflow rows as a short menu of their own: rows 1..k hold rows 7..6+k, the count is k, the start is where
// row 7 lies, no logo; everything is put back afterwards.
static void overflowMenu(uintptr_t obj,Saved& s,bool forHit){
 const int k=armed.count-baseRows;
 std::memcpy(s.row,rows(obj),sizeof(RowRecord)*size_t(k));s.count=*(int*)(obj+0x160);s.hover=*(int*)(obj+0x6c);s.pressed=*(int*)(obj+0x204);
 s.start=*(float*)(obj+0x200);s.logo=*(unsigned char*)(obj+0x216);
 const uintptr_t screen=*(uintptr_t*)(base+rva::guiDesktop);const float H=float(*(int*)(screen+0x2c));
 const float row=std::nearbyint(float(*(int*)(obj+0x1fc))**(float*)(obj+0x158));
 float y=std::nearbyint(H*s.start)+6.f*row;   // the draw adds a rounded row height per row
 if(forHit){y=std::nearbyint(H*s.start);for(int i=0;i<baseRows;++i)y=std::nearbyint(float(*(int*)(obj+0x1fc))**(float*)(obj+0x158)+y);}   // the hit test rounds the running sum
 for(int i=0;i<k;++i)rows(obj)[i]={textWidth(armed.final[baseRows+i].text),0,0,armed.final[baseRows+i].text};
 *(int*)(obj+0x160)=k;*(float*)(obj+0x200)=H>0.f?y/H:s.start;*(unsigned char*)(obj+0x216)=0;
 *(int*)(obj+0x6c)=s.hover>baseRows?s.hover-baseRows:0;*(int*)(obj+0x204)=s.pressed>baseRows?s.pressed-baseRows:0;
}
static void restore(uintptr_t obj,const Saved& s){
 const int k=armed.count-baseRows;
 std::memcpy(rows(obj),s.row,sizeof(RowRecord)*size_t(k));*(int*)(obj+0x160)=s.count;*(int*)(obj+0x6c)=s.hover;*(int*)(obj+0x204)=s.pressed;
 *(float*)(obj+0x200)=s.start;*(unsigned char*)(obj+0x216)=s.logo;
}
using MenuBuild=void(*)(int);using MenuDraw=void(*)(uintptr_t);using MenuHit=int(*)(uintptr_t,int,int,uintptr_t*,int*);
using MenuNav=uint32_t(*)(uintptr_t,uint32_t);using MenuActivate=uint64_t(*)(uintptr_t,uintptr_t,uintptr_t);
static MenuBuild originalMenuBuild=nullptr;static MenuDraw originalMenuDraw=nullptr;static MenuHit originalMenuHit=nullptr;
static MenuNav originalMenuNav=nullptr;static MenuActivate originalMenuActivate=nullptr;
static bool live(uintptr_t obj){return armed.on&&obj==armed.obj&&titleMenu(obj);}
static bool anyRows(){AcquireSRWLockShared(&extrasLock);bool any=false;for(const Extra& e:extras)any=any||(e.used&&e.enabled);ReleaseSRWLockShared(&extrasLock);return any;}
static void overflowDraw(uintptr_t obj){Saved s;bool set=false;__try{overflowMenu(obj,s,false);set=true;originalMenuDraw(obj);}__finally{if(set)restore(obj,s);}}
static int overflowHit(uintptr_t obj,int x,int y,uintptr_t* outObj,int* outIndex){
 Saved s;bool set=false;int r=0;__try{overflowMenu(obj,s,true);set=true;r=originalMenuHit(obj,x,y,outObj,outIndex);}__finally{if(set)restore(obj,s);}return r;
}
static void tryArm(uintptr_t obj){__try{arm(obj);}__except(EXCEPTION_EXECUTE_HANDLER){armed.on=false;}}
static void menuBuild(int mode){
 originalMenuBuild(mode);armed.on=false;releaseRetired();   // the builder wrote all six rows again
 uintptr_t obj=0;if(mode==0&&copyWorldMemory(base+rva::menuObject,&obj,8)&&titleMenu(obj)&&anyRows())tryArm(obj);
}
// Subtitles, drawn in the game's own small font right after the menu draw, with its own row: text is drawn upward
// from its y (0x27700/0x27180 put a glyph between y - height and y), so row k's label ends at its row y and the next
// label starts at y + row - [0x49a68c] (the title font's height). A subtitle sits centred in that gap when it fits
// there, else on the same line just right of its row's label; it never reaches another row. Layout as the draw used
// (0x128f30: row k at y = round(screen height * +0x200) + [0x49a690] + (k-1) * round(+0x1fc * +0x158), centred).
// The draw selects the game's text shader first, as the game's own text draws do (0x2143b0(2)): a hovered row leaves
// the highlight shader bound, and text drawn through it shows as a pale box. The previous mode is put back.
// 0x27080(font) selects a font, 0x27000(text) measures, 0x27180(x, y, text) draws. The font is the game's GUI font
// *[0x49a8e0] (the one its windows and buttons use, 0x2f060), which is resident at the title screen; the HUD's small
// font (0x49a8f0) is not loaded there yet and drew as a pale box of untextured glyph quads.
// the menu's title font and the previous current font are put back afterwards (docs/reviews/title-subtitle-review.md).
static bool subtitleFont=false;
static void drawSubtitles(uintptr_t obj){
 if(!subtitleFont||!armed.on||!readable(base+rva::guiDesktop,8)||!readable(base+0x49a690,4)||!readable(base+0x49a8e0,8)||!readable(base+0x49a670,8)||!readable(base+0x49a6b0,8))return;
 const uintptr_t screen=*(uintptr_t*)(base+rva::guiDesktop),font=*(uintptr_t*)(base+0x49a8e0);if(!screen||!font||!readable(screen+0x2c,4)||!readable(font+0x30,8))return;
 const float W=float(*(int*)(screen+0x28)),H=float(*(int*)(screen+0x2c));
 const float row=std::nearbyint(float(*(int*)(obj+0x1fc))**(float*)(obj+0x158));if(!(row>0.f)||!(H>0.f))return;
 float y=std::nearbyint(H**(float*)(obj+0x200))+float(*(int*)(base+0x49a690));
 const auto select=(void(*)(uintptr_t))(base+0x27080);const auto measure=(int(*)(uintptr_t))(base+0x27000);
 const auto draw=(uint64_t(*)(float,float,uintptr_t))(base+0x27180);const auto colour=(void(*)(int,int,int,int))(base+0x1f9ad0);
 const auto blend=(void(*)(unsigned))(base+0x73e90);const auto texture=(void(*)(uintptr_t))(base+0x214310);
 const auto shader=(void(*)(int))(base+0x2143b0);if(!readable(base+0x5b21b0,4))return;
 const uintptr_t previous=*(uintptr_t*)(base+0x49a670);const int previousShader=*(int*)(base+0x5b21b0);bool any=false;
 if(!readable(base+0x49a68c,4)||!readable(font+0xc,4))return;
 const float titleHeight=float(*(int*)(base+0x49a68c)),smallHeight=float(*(int*)(font+0xc));
 if(!(titleHeight>0.f)||!(smallHeight>0.f))return;const float gap=row-titleHeight;
 AcquireSRWLockShared(&extrasLock);
 for(int i=0;i<armed.count;++i){const int e=armed.extraIndex[i];
  if(e>=0&&e<maxExtra&&subtitles[e].len>0){
   if(!any){any=true;blend(0x103);shader(2);select(font);colour(0xc8,0xc0,0xb0,0xe0);}
   const uintptr_t s=(uintptr_t)subtitles[e].text;const float w=float(measure(s));
   if(gap>=smallHeight+2.f)draw(std::nearbyint((W-w)*0.5f),std::nearbyint(y+(gap-smallHeight)*0.5f+smallHeight),s);   // under its row
   else{const float x=std::nearbyint((W+float(armed.final[i].width))*0.5f+row*0.3f);if(x+w<=W-4.f)draw(x,y,s);}         // beside its label
  }
  y=std::nearbyint(y+row);
 }
 ReleaseSRWLockShared(&extrasLock);
 if(any){*(uintptr_t*)(base+0x49a670)=previous;texture(*(uintptr_t*)(base+0x49a6b0));colour(0xff,0xff,0xff,0xff);shader(previousShader);blend(0x203);}
}
static void menuDraw(uintptr_t obj){
 if(titleMenu(obj)){
  if(dirty.exchange(false)){disarm();releaseRetired();}   // registrations changed: rebuild below
  const bool want=anyRows();
  if(want&&!armed.on)tryArm(obj);else if(!want&&armed.on)disarm();
 }
 if(!live(obj)){originalMenuDraw(obj);return;}
 const int hover=*(int*)(obj+0x6c),pressed=*(int*)(obj+0x204);
 if(hover>baseRows)*(int*)(obj+0x6c)=0;if(pressed>baseRows)*(int*)(obj+0x204)=0;   // the game's own pass never indexes past row 6
 originalMenuDraw(obj);*(int*)(obj+0x6c)=hover;*(int*)(obj+0x204)=pressed;
 overflowDraw(obj);
 __try{drawSubtitles(obj);}__except(EXCEPTION_EXECUTE_HANDLER){subtitleFont=false;}
}
static bool(*formOpen)()=nullptr;   // settingstabs::optionsOpen, set by the plugin (that header comes later)
static int menuHit(uintptr_t obj,int x,int y,uintptr_t* outObj,int* outIndex){
 if(formOpen&&formOpen())return 0;   // the buttons are hidden while Settings is open
 const int r=originalMenuHit(obj,x,y,outObj,outIndex);if(r>0||!live(obj))return r;
 const int k=overflowHit(obj,x,y,outObj,outIndex);return k>=1&&k<=armed.count-baseRows?baseRows+k:r;
}
static uint32_t menuNav(uintptr_t obj,uint32_t key){
 if(live(obj)&&!(key&0x700)&&(key&3)&&(key&0xf000)==0x8000){
  const int hover=*(int*)(obj+0x6c);
  if(hover>=baseRows&&hover<armed.count)return uint32_t(((uint64_t(*)(uintptr_t,int))(base+rva::hoverTo))(obj,hover+1));   // down into / inside the extra rows
 }
 return originalMenuNav(obj,key);
}
static void runAction(Action action,void* context){
 const bool outer=insideActivate;insideActivate=true;__try{action(context);}__except(EXCEPTION_EXECUTE_HANDLER){}insideActivate=outer;
}
static uint64_t menuActivate(uintptr_t obj,uintptr_t a,uintptr_t b){
 if(live(obj)){
  const int i=*(int*)(obj+0x70);
  if(i>=1&&i<=armed.count){
   const int e=armed.extraIndex[i-1];
   if(e>=0){
    *(int*)(obj+0x70)=0;*(int*)(obj+0x204)=0;
    ++activating;Action action=nullptr;void* context=nullptr;int actsAs=0;
    AcquireSRWLockShared(&extrasLock);if(extras[e].used&&extras[e].enabled){action=extras[e].action;context=extras[e].context;actsAs=extras[e].actsAs;}ReleaseSRWLockShared(&extrasLock);
    if(action)runAction(action,context);
    --activating;
    if(!action||!actsAs)return 0;
    *(int*)(obj+0x70)=actsAs;return originalMenuActivate(obj,a,b);   // then the base row's own flow
   }
   *(int*)(obj+0x70)=armed.baseIndex[i-1];   // base rows keep the game's own actions
   if(onBaseRow)onBaseRow(armed.baseIndex[i-1]);
  }
 }
 return originalMenuActivate(obj,a,b);
}
// Exact entry bytes of the five targets (first instructions, no RIP-relative operand inside the 5-byte patch).
static bool untouched(uintptr_t p,const unsigned char* prefix,size_t size){return gamebridge::executable(p)&&readable(p,size)&&!std::memcmp((const void*)p,prefix,size);}
struct Target {uintptr_t rva;unsigned char prefix[12];size_t size;void* detour;void** original;const char* name;};
static const Target targets[]={
 {rva::menuBuild,{0x53,0x48,0x8d,0x64,0x24,0xe0,0x89,0xcb},8,(void*)menuBuild,(void**)&originalMenuBuild,"menu build 0x122c40"},
 {rva::menuHit,{0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0x5d},12,(void*)menuHit,(void**)&originalMenuHit,"menu hit 0x128d80"},
 {rva::menuNav,{0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x89},12,(void*)menuNav,(void**)&originalMenuNav,"menu nav 0x128a70"},
 {rva::menuActivate,{0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x31,0xf6},12,(void*)menuActivate,(void**)&originalMenuActivate,"menu activate 0x128580"}};
static SRWLOCK installLock=SRWLOCK_INIT;
// Made once, at the first registration. A failed enable leaves the created hooks disabled (never removed, so no
// trampoline is freed under a running call) and the rows unavailable.
static bool installHooks(){
 if(installed)return true;AcquireSRWLockExclusive(&installLock);
 if(!installed&&ready&&originalMenuDraw){
  // Checked again here: another mod may have hooked one of them since availability was decided.
  bool ok=true;for(const auto& t:targets)ok=ok&&untouched(base+t.rva,t.prefix,t.size);
  if(!ok)ready=false;
  if(ok)for(const auto& t:targets){const auto s=MH_CreateHook((void*)(base+t.rva),t.detour,t.original);if(s!=MH_OK&&s!=MH_ERROR_ALREADY_CREATED){ok=false;break;}}
  if(ok)for(const auto& t:targets){const auto s=MH_EnableHook((void*)(base+t.rva));if(s!=MH_OK&&s!=MH_ERROR_ENABLED){ok=false;break;}}
  if(ok)installed=true;else{for(const auto& t:targets)MH_DisableHook((void*)(base+t.rva));ready=false;}
 }
 ReleaseSRWLockExclusive(&installLock);return installed;
}
// Whether rows can be offered: every target untouched (the draw before the runtime's own presentation detour
// took it), the helpers matching. Nothing is hooked here.
static std::string available(bool presentationOwnsDraw){
 if(ready)return {};
 if(!presentationOwnsDraw||!drawWasOriginal)return "menu draw 0x128f30 is not the runtime's own (another mod hooked it first, or the lifecycle is unavailable)";
 for(const auto& t:targets)if(!untouched(base+t.rva,t.prefix,t.size))return std::string(t.name)+" is already detoured or differs; title rows are left to the mod that owns it";
 const unsigned char measurePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x8b,0x05};
 const unsigned char hoverPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x89,0x53,0x6c,0x85,0xd2,0x0f,0x8e};
 const unsigned char assignPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6,0x48,0x3b,0x33};
 const unsigned char widthPrefix[]={0x31,0xc0,0x48,0x89,0xca,0x48,0x85,0xc9,0x74,0x04,0x48,0x8b,0x52,0xf8,0x83,0xfa,0x01};
 if(!gamebridge::operation(base+rva::measure,measurePrefix,sizeof measurePrefix)||!gamebridge::operation(base+rva::hoverTo,hoverPrefix,sizeof hoverPrefix)||
    !gamebridge::operation(base+rva::assign,assignPrefix,sizeof assignPrefix)||!gamebridge::operation(base+rva::width,widthPrefix,sizeof widthPrefix)||
    !readable(base+rva::menuObject,8)||!readable(base+rva::guiDesktop,8))return "a title-menu helper function did not match";
 {const unsigned char selectPrefix[]={0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xc8,0x48,0x89,0x05};
  const unsigned char drawPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0x98,0x66,0x0f};
  const unsigned char measurePrefix2[]={0x31,0xc0,0x48,0x89,0xca,0x48,0x85,0xc9,0x74,0x04,0x48,0x8b};
  const unsigned char colourPrefix[]={0x88,0x0d};const unsigned char blendPrefix[]={0x48,0x8d,0x64,0x24,0xd8,0x89,0x4c,0x24,0x20,0x89,0xc8,0x3b};
  const unsigned char texturePrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xc8,0x48,0x3b,0x05};
  const unsigned char shaderPrefix[]={0x48,0x8d,0x64,0x24,0xd8,0x89,0xc8,0x3b,0x05};   // 0x2143b0 shader mode
  subtitleFont=gamebridge::operation(base+0x27080,selectPrefix,sizeof selectPrefix)&&gamebridge::operation(base+0x27180,drawPrefix,sizeof drawPrefix)&&
   gamebridge::operation(base+0x27000,measurePrefix2,sizeof measurePrefix2)&&gamebridge::operation(base+0x1f9ad0,colourPrefix,sizeof colourPrefix)&&
   gamebridge::operation(base+0x73e90,blendPrefix,sizeof blendPrefix)&&gamebridge::operation(base+0x214310,texturePrefix,sizeof texturePrefix)&&
   gamebridge::operation(base+0x2143b0,shaderPrefix,sizeof shaderPrefix);}
 ready=true;return {};
}
}
