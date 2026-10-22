#pragma once
#include <cwchar>
#include <emmintrin.h>
#include <functional>
#include <utility>
// Shared owner of the game's Settings form (exports add_settings_tab & co.): extra tabs beside Graphics and
// Controls, and extra rows under the native Controls list, built from rows the registered tabs supply. This is
// ExCamera's reviewed native Settings integration (E:\ExCamera\src\native_settings.cpp, mod\panel.inl) made generic:
// the same form/element contract, element tables replaced through the game's own copy/set/free, the same Apply,
// Cancel and Defaults sequencing and checkbox coordinate correction. It runs from one detour of the GUI
// maintenance step 0x32180, installed only on its original bytes: ExCamera registers its Camera, Combat and VR tabs
// here when this runtime is present and keeps its own hook only when it is not, so the form has one owner.
namespace settingstabs {
// ---- the game's GUI contract (ExCamera game_structs.h / game_signatures.h) ----
struct NativeElement {
 uint16_t kind=1,reserved=0;int16_t x=0,y=0,width=0,height=0,style=0,group=0;const char* text=nullptr;
 uint32_t appearance=0x403300,flags=0;uintptr_t owner=0;void(*action)(NativeElement*)=nullptr;uintptr_t handler=0;
 float position=0;uint32_t spare=0;uintptr_t help=0;int64_t identity=0;uint64_t data[3]{};
};
static_assert(sizeof(NativeElement)==0x68&&offsetof(NativeElement,text)==0x10&&offsetof(NativeElement,owner)==0x20&&offsetof(NativeElement,data)==0x50);
// The game's literal string layout: reference count -1, length, then the bytes (code page 1252).
struct NativeText {int64_t references=-1,length=0;char bytes[256]{};
 void set(const std::wstring& v){const int n=WideCharToMultiByte(1252,WC_NO_BEST_FIT_CHARS,v.c_str(),-1,bytes,int(sizeof bytes),"?",nullptr);if(!n)bytes[0]=0;length=n?n-1:0;}};
namespace form {constexpr size_t contentLeft=0x30,contentTop=0x40,contentRight=0x44,contentBottom=0x3C,contentWidth=0x48,horizontalScroll=0xD8,verticalScroll=0xDC,
 scrollExtent=0x4C,elementLast=0x90,elements=0x98,x=0x20,y=0x24,width=0x28,height=0x2C,frameFlags=0x64;}
namespace options {constexpr size_t graphicsPane=0x158,controlsPane=0x160,controllerPane=0x170,applied=0x138;}
namespace rva {constexpr uintptr_t maintenance=0x32180,formElements=0x2B200,formClose=0x2C410,elementsCopy=0x32860,elementsFree=0x2AA90,
 settingsTab=0xA7FC0,controlsTab=0xA7FF0,formInvalidate=0x2E760,formShow=0x2C8F0,formGeometry=0x2BCC0,optionsApply=0xA6770,optionsCancel=0xA8390,
 optionsDefaults=0xA8AF0,checkboxHandler=0x33020,requestRedraw=0x26140,desktop=0x49A930,optionsForm=0x49CD50,optionsElements=0x2B2AF0,
 controlsElements=0x2B1700,controllerElements=0x2B28E0,uiScale=0x2AD3F0;}
constexpr unsigned buttonTransient=0x100000,inactiveFlags=0x110,checkedFlags=0x60;

// ---- the provider ABI (exports; extern "C", default x64 convention) ----
// kind: 0 divider (section title), 1 checkbox, 2 slider (steps, position), 3 value button (a click asks for the
// next value), 4 key binding (a click starts capture), 5 text line.
struct Row {int32_t id=-1,kind=5;const wchar_t* label=nullptr;const wchar_t* value=nullptr;int32_t steps=0;float position=0;int32_t checked=0,inactive=0;};
using RowsFn=int(*)(void* context,int page,Row* out,int capacity);   // page: 0 the tab's own page, 1 extra rows under native Controls
using ChangeFn=void(*)(void* context,int page,int id,int kind,int delta,float position,int checked,int key);
using EventFn=void(*)(void* context,int event);   // 0 opened, 1 applied, 2 cancelled, 3 defaults on this tab, 4 closed
struct Tab {std::wstring title;void* context=nullptr;RowsFn rows=nullptr;ChangeFn change=nullptr;EventFn event=nullptr;bool controls=false;};
static SRWLOCK tabsLock=SRWLOCK_INIT;static std::vector<Tab> tabs;static std::atomic<bool> rebuildWanted{false},ready{false},hooked{false};
constexpr size_t maxTabs=5;   // beside Graphics and Controls

// ---- the engine (one Settings form at a time, game thread only) ----
constexpr int capacity=192,none=-1,pageEvent=1000,nativeWidth=434,nativeHeight=536;
struct Table {NativeElement* data=nullptr;int count=0;};
struct Frame {int x=0,y=0,width=0,height=0;};
struct Action {int tab=none,row=none,kind=0,delta=0;bool slider=false,capture=false;};
struct Engine {
 uintptr_t root=0,graphics=0,controls=0,controller=0;bool widened=false,fault=false,dirty=false,controlsOwned=false,closing=false,applyQueued=false,
  cancelQueued=false,baseSaved=false,scrollOwned=false,originalScrolling=false,updating=false;
 int page=0,rootCount=0,controlCount=0,baseCount=44,originalExtent=0,extentScale=4,builtPage=-1,widthGain=0,tabCount=2;
 unsigned callbackDepth=0,handlerDepth=0;DWORD thread=0;
 std::array<Frame,4> originalFrames{};std::array<NativeElement,44> originalControls{};std::array<void(*)(NativeElement*),9> originals{};
 std::array<Action,capacity> rootActions{},controlActions{};std::array<NativeText,capacity> rootText{},controlText{};
 int capture=none,captureTab=none,captureId=none;std::vector<Tab> shown;   // tabs as of the last rebuild
};
static Engine engine;
template<class T>static T at(uintptr_t a){return *(T*)a;}
static bool contains(uintptr_t f){
 const uintptr_t desktop=at<uintptr_t>(base+rva::desktop);if(!desktop||!readable(desktop,0xB8))return false;
 const int n=at<int>(desktop+0xA8);const uintptr_t list=at<uintptr_t>(desktop+0xB0);
 if(n<0||n>4096||(n&&!readable(list,size_t(n)*8)))return false;for(int i=0;i<n;++i)if(((uintptr_t*)list)[i]==f)return true;return false;
}
// Whether the game's Settings form is open (on the desktop's form list): the title menu is hidden behind it.
static bool optionsOpen(){
 __try{if(!readable(base+rva::optionsForm,8))return false;const uintptr_t f=at<uintptr_t>(base+rva::optionsForm);return f&&contains(f);}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static Table table(uintptr_t f){
 if(!f||!readable(f,form::elements+8))return {};const int last=at<int>(f+form::elementLast);if(last<0||last>=capacity)return {};
 auto e=at<NativeElement*>(f+form::elements);if(!readable(uintptr_t(e),size_t(last+1)*sizeof(NativeElement)))return {};return {e,last+1};
}
static Table basis(uintptr_t r,int n){auto e=(NativeElement*)(base+r);return readable(uintptr_t(e),size_t(n)*sizeof(NativeElement))?Table{e,n}:Table{};}
static void replace(uintptr_t f,const NativeElement* source,int n){
 using Copy=void(*)(const NativeElement*,int,NativeElement**,int*);using Free=void(*)(NativeElement**,int);using Set=void(*)(uintptr_t,const NativeElement*,int,unsigned char);
 NativeElement* retained=nullptr;int last=-1;((Copy)(base+rva::elementsCopy))(source,n-1,&retained,&last);
 __try{((Set)(base+rva::formElements))(f,retained,last,0);}__finally{((Free)(base+rva::elementsFree))(&retained,last);}
 ((void(*)(uintptr_t))(base+rva::formInvalidate))(f);
}
static void geometry(uintptr_t f){((void(*)(uintptr_t))(base+rva::formGeometry))(f);}
static int uiScale(){const int v=at<int>(base+rva::uiScale);return v<1||v>32?4:v;}
static bool frame(uintptr_t f,Frame& o){if(!readable(f,form::height+4))return false;o={at<int>(f+form::x),at<int>(f+form::y),at<int>(f+form::width),at<int>(f+form::height)};return true;}
static void setFrame(uintptr_t f,const Frame& i){*(int*)(f+form::x)=i.x;*(int*)(f+form::y)=i.y;*(int*)(f+form::width)=i.width;*(int*)(f+form::height)=i.height;geometry(f);}
static void fail(){engine.fault=true;}
static NativeElement unscaled(NativeElement live,const NativeElement& c){live.x=c.x;live.y=c.y;live.width=c.width;live.height=c.height;live.style=c.style;live.flags&=~0x1000u;return live;}
static bool nativeRoot(Table l){
 const auto s=basis(rva::optionsElements,9);if(!l.data||!s.data||l.count!=9)return false;
 if(uintptr_t(s.data[0].action)!=base+rva::settingsTab||uintptr_t(s.data[1].action)!=base+rva::controlsTab||uintptr_t(s.data[7].action)!=base+rva::optionsApply)return false;
 for(int i=0;i<9;++i)if(l.data[i].action!=s.data[i].action||l.data[i].kind!=s.data[i].kind||l.data[i].owner!=l.data[0].owner)return false;
 return l.data[0].width>0&&(l.data[0].flags&0x1000);
}
static bool nativeList(Table t,uintptr_t r,int n){const auto s=basis(r,n);if(!t.data||!s.data||t.count!=n)return false;for(int i=0;i<n;++i)if(t.data[i].kind!=s.data[i].kind||t.data[i].action!=s.data[i].action)return false;return true;}
static bool nativeKeyboard(Table t){return nativeList(t,rva::controlsElements,44);}
static bool nativeController(Table t){return nativeList(t,rva::controllerElements,5);}
template<int Index,bool Controls>static void activated(NativeElement* e);
template<bool Controls,size_t... I>static auto callbacks(std::index_sequence<I...>){return std::array<void(*)(NativeElement*),sizeof...(I)>{activated<int(I),Controls>...};}
static bool ownedRoot(Table t){
 if(engine.dirty&&t.count==9)return nativeRoot(t);
 static const auto f=callbacks<false>(std::make_index_sequence<capacity>{});
 return t.data&&t.count==engine.rootCount&&t.count>=10&&t.data[0].action==f[0]&&t.data[7].action==f[7]&&t.data[9].text==engine.rootText[9].bytes;
}
static bool ownedControls(Table t){return t.data&&t.count==engine.controlCount&&t.count>engine.baseCount&&t.data[engine.baseCount].text==engine.controlText[engine.baseCount].bytes&&t.data[engine.baseCount].owner==engine.controls;}
template<bool Controls>static void add(std::vector<NativeElement>& out,Action action,int x,int y,int width,const std::wstring& text,unsigned kind,unsigned flags,bool slider=false,float position=0,int steps=0){
 const size_t i=out.size();if(i>=capacity||width<=0){fail();return;}
 auto& words=Controls?engine.controlText:engine.rootText;auto& actions=Controls?engine.controlActions:engine.rootActions;
 words[i].set(text);actions[i]=action;actions[i].slider=slider;
 NativeElement e;e.kind=uint16_t(kind);e.x=int16_t(x);e.y=int16_t(y);e.width=int16_t(width);e.text=words[i].bytes;e.flags=flags;
 e.appearance=kind==1?0x20403300:kind==4?0x00405320:0;e.owner=Controls?engine.controls:engine.root;
 if(slider){e.position=position;e.data[0]=uint64_t((std::max)(steps,1));}
 static const auto functions=callbacks<Controls>(std::make_index_sequence<capacity>{});
 e.action=action.row==none&&action.tab<pageEvent?nullptr:functions[i];if(action.kind==0&&action.row==none&&action.tab<pageEvent)e.action=nullptr;
 out.push_back(e);
}
static void divider(std::vector<NativeElement>& out,const std::wstring& title,bool first){add<true>(out,{},52,first?2:-32760,324+engine.widthGain,title,5,0);out.back().appearance=0x3300;}
// One provider row as the game's widgets (ExCamera's row(): checkbox 2, label 5 + slider 4 + value 5, button 1).
static void row(std::vector<NativeElement>& out,int tab,const Row& r){
 const unsigned flags=r.inactive?0x10:0;const int gain=engine.widthGain;const std::wstring label=r.label?r.label:L"",value=r.value?r.value:L"";
 Action a;a.tab=tab;a.row=r.id;a.kind=r.kind;
 switch(r.kind){
  case 0:divider(out,label,false);return;
  case 1:a.delta=1;add<true>(out,a,80,-32760,292+gain,label,2,flags|(r.checked?checkedFlags:0));return;
  case 2:add<true>(out,{},16,-32760,148,label,5,flags);a.delta=1;add<true>(out,a,176,-32768,116+gain,L"",4,flags,true,r.position,r.steps);
   add<true>(out,{},300+gain,-32768,70,value.substr(0,10),5,flags);out.back().appearance=0x3300;return;
  case 3:add<true>(out,{},16,-32760,148,label,5,flags);a.delta=1;add<true>(out,a,176,-32768,194+gain,value,1,flags);return;
  case 4:{add<true>(out,{},80,-32760,124,label,5,flags);a.capture=true;
   const bool capturing=engine.capture!=none&&engine.captureTab==tab&&engine.captureId==r.id;
   add<true>(out,a,232,-32768,136+gain,capturing?L"Press key...":value,5,flags);out.back().appearance=0x20003380;return;}
  default:add<true>(out,{},16,-32760,300+gain,label+(value.empty()?L"":L"  "+value),5,flags);return;
 }
}
static int callRows(const Tab& t,int page,Row* out,int count){__try{return t.rows?t.rows(t.context,page,out,count):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
static std::vector<Row> rowsOf(const Tab& t,int page){
 std::vector<Row> rows(128);const int n=callRows(t,page,rows.data(),int(rows.size()));
 rows.resize(size_t((std::max)(0,(std::min)(n,128))));return rows;
}
static void tabEvent(const Tab& t,int event){__try{if(t.event)t.event(t.context,event);}__except(EXCEPTION_EXECUTE_HANDLER){}}
static void tabChange(const Tab& t,int page,int id,int kind,int delta,float position,int checked,int key){__try{if(t.change)t.change(t.context,page,id,kind,delta,position,checked,key);}__except(EXCEPTION_EXECUTE_HANDLER){}}
static void widen(){
 if(engine.widened)return;Frame r{},g{},c{},k{};
 if(!frame(engine.root,r)||!frame(engine.graphics,g)||!frame(engine.controls,c)||(engine.controller&&!frame(engine.controller,k)))return;
 const double s=(r.width-g.width)/8.0;constexpr int unplaced=int(0xC521974Fu);
 if(s<.9||s>8||std::abs(r.width-nativeWidth*s)>4||std::abs(r.height-nativeHeight*s)>4||std::abs((r.width-c.width)-16*s)>2||r.x==unplaced||r.y==unplaced)return;
 engine.originalFrames={r,g,c,k};const int gain=int(engine.widthGain*s+.5);
 r.x-=gain/2;r.width+=gain;setFrame(engine.root,r);g.width+=gain;setFrame(engine.graphics,g);c.width+=gain;setFrame(engine.controls,c);
 if(engine.controller){k.width+=gain;setFrame(engine.controller,k);}engine.widened=true;
}
static void narrow(){if(!engine.widened)return;engine.widened=false;if(!contains(engine.root))return;
 setFrame(engine.root,engine.originalFrames[0]);setFrame(engine.graphics,engine.originalFrames[1]);setFrame(engine.controls,engine.originalFrames[2]);if(engine.controller)setFrame(engine.controller,engine.originalFrames[3]);}
static void rebuildRoot(){
 auto t=table(engine.root),c=basis(rva::optionsElements,9);if(!ownedRoot(t)||c.count!=9){fail();return;}
 std::vector<NativeElement> records;for(int i=0;i<9;++i)records.push_back(unscaled(t.data[i],c.data[i]));
 static const auto f=callbacks<false>(std::make_index_sequence<capacity>{});for(int i:{0,1,6,7,8})records[size_t(i)].action=f[size_t(i)];
 for(int i=0;i<9;++i)engine.rootActions[size_t(i)]={};
 const int x=c.data[0].x,gain=engine.widened?engine.widthGain:0,gap=c.data[1].x-x-c.data[0].width,span=c.data[1].x+c.data[1].width-x+gain;
 const int width=((span-(engine.tabCount-1)*gap)/engine.tabCount/4)*4;if(gap<0||width<32){fail();return;}
 engine.rootText[0].set(L"Graphics");records[0].text=engine.rootText[0].bytes;engine.rootText[1].set(L"Controls");records[1].text=engine.rootText[1].bytes;
 for(int i:{4,5})records[size_t(i)].width=int16_t(records[size_t(i)].width+gain);records[6].x=int16_t(records[6].x+gain/2);
 for(int i=0;i<2;++i){records[size_t(i)].x=int16_t(x+i*(width+gap));records[size_t(i)].width=int16_t(width);records[size_t(i)].flags=(records[size_t(i)].flags&~0x60u)|(engine.page==i?0x60:0);}
 if(engine.page>=2)for(int i=2;i<6;++i)records[size_t(i)].flags|=0x110;
 for(int i=2;i<engine.tabCount;++i){Action a;a.tab=pageEvent+i;add<false>(records,a,x+i*(width+gap),c.data[0].y,width,engine.shown[size_t(i-2)].title,1,engine.page==i?0x60:0);
  auto& e=records.back();e.height=c.data[0].height;e.group=1;e.appearance=c.data[0].appearance;}
 if(engine.fault||records.size()>capacity){fail();return;}
 engine.rootCount=int(records.size());replace(engine.root,records.data(),engine.rootCount);
 if(engine.page>=2){((void(*)(uintptr_t,unsigned char))(base+rva::formShow))(engine.graphics,0);((void(*)(uintptr_t,unsigned char))(base+rva::formShow))(engine.controls,1);
  if(engine.controller)((void(*)(uintptr_t,unsigned char))(base+rva::formShow))(engine.controller,0);}
}
static int baselineExtent(){return int(int64_t(engine.originalExtent)*uiScale()/engine.extentScale);}
static void setScrolling(uintptr_t pane,bool on){auto& f=*(unsigned*)(pane+form::frameFlags);const unsigned u=(f&~0x200u)|(on?0x200u:0u);if(u!=f){f=u;geometry(pane);}}
static void setExtent(uintptr_t pane,int h){*(int*)(pane+form::scrollExtent)=h;geometry(pane);}
static void installCheckboxHandlers();
static void rebuildControls(Table live){
 if(!engine.controlsOwned)engine.baseCount=nativeController(live)?5:44;
 const auto c=engine.baseCount==5?basis(rva::controllerElements,5):basis(rva::controlsElements,44);
 if(c.count!=engine.baseCount||(!engine.controlsOwned&&!nativeKeyboard(live)&&!nativeController(live))||(engine.controlsOwned&&!ownedControls(live))){fail();return;}
 if(!engine.controlsOwned){engine.originalExtent=at<int>(engine.controls+form::scrollExtent);engine.extentScale=uiScale();}
 if(!engine.baseSaved||!engine.controlsOwned||engine.builtPage==1){for(int i=0;i<engine.baseCount;++i)engine.originalControls[size_t(i)]=unscaled(live.data[i],c.data[i]);engine.baseSaved=true;}
 std::vector<NativeElement> records(engine.originalControls.begin(),engine.originalControls.begin()+engine.baseCount);
 for(int i=0;i<engine.baseCount;++i)engine.controlActions[size_t(i)]={};
 // Rows under native Controls come from tabs registered for it; none means the native page stays as it is.
 std::vector<std::pair<int,std::vector<Row>>> extra;
 if(engine.page==1&&engine.baseCount==44)for(size_t i=0;i<engine.shown.size();++i)if(engine.shown[i].controls){auto r=rowsOf(engine.shown[i],1);if(!r.empty())extra.push_back({int(i),std::move(r)});}
 if(engine.page==0||(engine.page==1&&(engine.baseCount==5||extra.empty()))){
  if(engine.controlsOwned){replace(engine.controls,records.data(),engine.baseCount);setExtent(engine.controls,baselineExtent());}
  if(engine.scrollOwned)setScrolling(engine.controls,engine.originalScrolling);engine.scrollOwned=false;engine.controlsOwned=false;engine.builtPage=0;return;
 }
 if(!engine.scrollOwned){engine.originalScrolling=(at<unsigned>(engine.controls+form::frameFlags)&0x200)!=0;engine.scrollOwned=true;}setScrolling(engine.controls,true);
 if(engine.page>=2)for(auto& e:records)e.flags|=0x110;
 int lines=0,extent=baselineExtent();
 if(engine.page==1){for(auto& [tab,rows]:extra)for(const auto& r:rows){row(records,tab,r);++lines;}extent=baselineExtent()+(lines+2)*8*uiScale();}
 else{const int tab=engine.page-2;bool first=true;
  for(const auto& r:rowsOf(engine.shown[size_t(tab)],0)){if(r.kind==0){divider(records,r.label?r.label:L"",first);}else row(records,tab,r);first=false;++lines;}
  extent=(lines+4)*8*uiScale();}
 if(engine.fault||records.size()>capacity){fail();return;}
 engine.controlCount=int(records.size());replace(engine.controls,records.data(),engine.controlCount);setExtent(engine.controls,extent);
 engine.builtPage=engine.page;engine.controlsOwned=true;installCheckboxHandlers();
}
// ---- callbacks from the game's widgets ----
using NativeHandler=int(*)(NativeElement*,unsigned,uint32_t,uint32_t);
static bool scrollDisplacement(int32_t extent,int32_t begin,int32_t end,float fraction,int64_t& out){
 if(extent<0||int64_t(end)<=int64_t(begin)||!std::isfinite(fraction))return false;const int64_t d=int64_t(extent)-(int64_t(end)-int64_t(begin));
 const __m128 p=_mm_mul_ss(_mm_cvtsi64_ss(_mm_setzero_ps(),d),_mm_set_ss(fraction));const float v=_mm_cvtss_f32(p);
 if(!std::isfinite(v)||v<-1073741824.f||v>1073741824.f)return false;out=_mm_cvtss_si64(p);return true;
}
// The native checkbox hit test ignores the pane's scroll; correct the packed point as ExCamera does.
static bool correctPoint(uintptr_t pane,uint32_t& packed){
 if(!readable(pane,form::verticalScroll+4))return false;int64_t sx=0,sy=0;
 if(!scrollDisplacement(at<int32_t>(pane+form::contentWidth),at<int32_t>(pane+form::contentLeft),at<int32_t>(pane+form::contentRight),at<float>(pane+form::horizontalScroll),sx)||
    !scrollDisplacement(at<int32_t>(pane+form::scrollExtent),at<int32_t>(pane+form::contentTop),at<int32_t>(pane+form::contentBottom),at<float>(pane+form::verticalScroll),sy))return false;
 const int64_t x=int16_t(uint16_t(packed))-2*sx,y=int16_t(uint16_t(packed>>16))-2*sy;if(x<-32768||x>32767||y<-32768||y>32767)return false;
 packed=uint32_t(uint16_t(x))|(uint32_t(uint16_t(y))<<16);return true;
}
template<int Index>static int checkboxEvent(NativeElement* e,unsigned event,uint32_t first,uint32_t second){
 if(engine.thread!=GetCurrentThreadId())return 0;++engine.handlerDepth;int result=0;
 __try{
  const auto a=engine.controlActions[Index];
  if(e&&engine.controlsOwned&&e->owner==engine.controls&&a.kind==1){
   uint32_t& point=event==0x30?first:second;
   if((event==0x30||event==0x50)&&!correctPoint(engine.controls,point))result=0;
   else result=((NativeHandler)(base+rva::checkboxHandler))(e,event,first,second);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){fail();}
 --engine.handlerDepth;return result;
}
template<size_t... I>static auto checkboxHandlers(std::index_sequence<I...>){return std::array<NativeHandler,sizeof...(I)>{checkboxEvent<int(I)>...};}
static void installCheckboxHandlers(){
 const auto live=table(engine.controls);if(!ownedControls(live))return;
 static const auto handlers=checkboxHandlers(std::make_index_sequence<capacity>{});
 for(int i=engine.baseCount;i<live.count;++i){if(engine.controlActions[size_t(i)].kind!=1||live.data[i].kind!=2)continue;
  const uintptr_t adapter=reinterpret_cast<uintptr_t>(handlers[size_t(i)]);
  if(live.data[i].handler!=base+rva::checkboxHandler&&live.data[i].handler!=adapter){fail();return;}live.data[i].handler=adapter;}
}
static void dispatch(int index,bool controls,NativeElement* e){
 const uintptr_t owner=controls?engine.controls:engine.root;if(!e||e->owner!=owner)return;
 const auto live=table(owner);if(!live.data||index>=live.count||&live.data[index]!=e)return;
 const auto a=controls?engine.controlActions[size_t(index)]:engine.rootActions[size_t(index)];
 if(!controls&&index<9){
  if(e->flags&inactiveFlags)return;
  if(index==0||index==1){engine.originals[size_t(index)](e);engine.page=index;engine.capture=none;engine.dirty=true;}
  else if(index==7){engine.closing=true;engine.capture=none;engine.originals[7](e);
   if(!contains(owner)){fail();return;}
   if(readable(owner,options::applied+4)&&at<int>(owner+options::applied)==1)engine.applyQueued=true;else engine.closing=false;}
  else if(index==8){engine.closing=true;engine.capture=none;engine.cancelQueued=true;engine.originals[8](e);}
  else if(index==6){if(engine.page<2)engine.originals[6](e);if(engine.page>=2)tabEvent(engine.shown[size_t(engine.page-2)],3);engine.dirty=true;}
  return;
 }
 if(a.tab>=pageEvent&&a.tab<pageEvent+8){engine.page=a.tab-pageEvent;engine.capture=none;engine.dirty=true;return;}
 if(a.tab<0||size_t(a.tab)>=engine.shown.size()||(e->flags&inactiveFlags))return;
 const auto& tab=engine.shown[size_t(a.tab)];const int page=engine.page==1?1:0;
 if(a.capture){if(e->kind!=5)return;engine.capture=index;engine.captureTab=a.tab;engine.captureId=a.row;engine.dirty=true;return;}
 if(a.kind==1){if(e->kind!=2)return;tabChange(tab,page,a.row,1,0,0,(e->flags&0x20)!=0,0);}
 else if(a.kind==2){if(e->kind!=4)return;tabChange(tab,page,a.row,2,0,e->position,0,0);engine.dirty=true;}
 else if(a.kind==3){if(e->kind!=1||!(e->flags&buttonTransient))return;tabChange(tab,page,a.row,3,1,0,0,0);engine.dirty=true;}
}
template<int Index,bool Controls>static void activated(NativeElement* e){
 if(engine.callbackDepth||engine.updating||engine.fault||engine.closing||engine.thread!=GetCurrentThreadId())return;
 ++engine.callbackDepth;__try{dispatch(Index,Controls,e);}__except(EXCEPTION_EXECUTE_HANDLER){fail();}--engine.callbackDepth;
}
// ---- the maintenance step ----
static bool callbackActive(){return engine.callbackDepth||engine.handlerDepth;}
static void broadcast(int event){for(const auto& t:engine.shown)tabEvent(t,event);}
static void restore(){
 if(callbackActive()||!ownedRoot(table(engine.root)))return;
 auto ct=table(engine.controls);
 if(engine.controlsOwned&&ownedControls(ct)&&engine.baseSaved){replace(engine.controls,engine.originalControls.data(),engine.baseCount);setExtent(engine.controls,baselineExtent());if(engine.scrollOwned)setScrolling(engine.controls,engine.originalScrolling);}
 const auto live=table(engine.root),c=basis(rva::optionsElements,9);std::array<NativeElement,9> original;
 for(int i=0;i<9;++i){original[size_t(i)]=unscaled(live.data[i],c.data[i]);original[size_t(i)].action=engine.originals[size_t(i)];}
 original[0].text=c.data[0].text;original[1].text=c.data[1].text;narrow();
 if(engine.page>=2){original[0].flags|=0x60;original[1].flags&=~0x60u;}
 replace(engine.root,original.data(),9);
 if(engine.page>=2&&!engine.closing){auto stock=table(engine.root);engine.originals[0](&stock.data[0]);}
 engine.root=0;engine.controlsOwned=false;engine.scrollOwned=false;engine.baseSaved=false;engine.capture=none;
}
static void retire(){if(callbackActive())return;engine.root=0;engine.controlsOwned=false;engine.scrollOwned=false;engine.baseSaved=false;engine.dirty=false;engine.capture=none;broadcast(4);}
// Key capture by polling (the runtime owns no window procedure): Escape cancels, Delete clears, F6..F11 ignored.
static void pollCapture(){
 if(engine.capture==none)return;
 for(int vk=1;vk<255;++vk){if(vk==VK_LBUTTON||vk==VK_RBUTTON)continue;if(!(GetAsyncKeyState(vk)&1))continue;
  if(vk>=VK_F6&&vk<=VK_F11)continue;const int key=vk==VK_ESCAPE?-1:vk==VK_DELETE?0:vk;
  if(key!=-1&&size_t(engine.captureTab)<engine.shown.size())tabChange(engine.shown[size_t(engine.captureTab)],engine.page==1?1:0,engine.captureId,4,0,0,0,key);
  engine.capture=none;engine.dirty=true;return;}
}
static void serviceTick(){
 if(engine.fault||callbackActive())return;if(!engine.thread)engine.thread=GetCurrentThreadId();if(engine.thread!=GetCurrentThreadId()){fail();return;}
 if(engine.applyQueued){engine.applyQueued=false;broadcast(1);}
 if(engine.cancelQueued){engine.cancelQueued=false;broadcast(2);}
 if(engine.root&&!contains(engine.root))retire();
 if(engine.root&&engine.closing)return;
 if(!engine.root){
  {AcquireSRWLockShared(&tabsLock);engine.shown=tabs;ReleaseSRWLockShared(&tabsLock);}
  bool ownTab=false;for(const auto& t:engine.shown)ownTab=ownTab||!t.controls;bool any=!engine.shown.empty();if(!any)return;
  const uintptr_t candidate=at<uintptr_t>(base+rva::optionsForm);if(!candidate||!contains(candidate))return;
  const auto t=table(candidate);if(!nativeRoot(t)||t.data[0].owner!=candidate)return;
  engine.root=candidate;if(!readable(candidate,options::controllerPane+8)){fail();return;}
  engine.graphics=at<uintptr_t>(candidate+options::graphicsPane);engine.controls=at<uintptr_t>(candidate+options::controlsPane);engine.controller=at<uintptr_t>(candidate+options::controllerPane);
  if(!engine.graphics||!engine.controls){fail();return;}
  engine.tabCount=2;for(const auto& t2:engine.shown)if(!t2.controls)++engine.tabCount;
  // Keep only tab pages in `shown` order; Controls-row providers stay in the list but get no tab.
  std::stable_partition(engine.shown.begin(),engine.shown.end(),[](const Tab& x){return !x.controls;});
  engine.widthGain=engine.tabCount<=2?0:engine.tabCount==3?50:engine.tabCount<=5?100:150;engine.widened=false;widen();
  engine.closing=false;for(int i=0;i<9;++i)engine.originals[size_t(i)]=t.data[i].action;engine.page=(t.data[1].flags&0x20)?1:0;engine.dirty=true;broadcast(0);(void)ownTab;
 }
 if(!ownedRoot(table(engine.root))){fail();return;}
 pollCapture();
 if(rebuildWanted.exchange(false))engine.dirty=true;
 auto ct=table(engine.controls);
 if(engine.controlsOwned&&!ownedControls(ct)){if(!nativeKeyboard(ct)&&!nativeController(ct)){fail();return;}engine.controlsOwned=false;engine.scrollOwned=false;engine.baseSaved=false;engine.dirty=true;}
 if(engine.dirty&&(GetAsyncKeyState(VK_LBUTTON)&0x8000))return;   // a native slider drag holds its element
 if(engine.dirty){rebuildRoot();if(engine.fault)return;if(nativeKeyboard(ct)||nativeController(ct)||engine.controlsOwned)rebuildControls(ct);engine.dirty=false;}
}
using Maintenance=void(*)(uintptr_t);static Maintenance originalMaintenance=nullptr;static thread_local bool inMaintenance=false;
static void serviceGuarded(){__try{engine.updating=true;serviceTick();engine.updating=false;}__except(EXCEPTION_EXECUTE_HANDLER){engine.updating=false;fail();}}
static void observe(){__try{if(engine.root&&!engine.fault&&!callbackActive()){if(!contains(engine.root))retire();else if(!engine.closing)installCheckboxHandlers();}}__except(EXCEPTION_EXECUTE_HANDLER){fail();}}
static void maintenance(uintptr_t self){
 if(engine.fault||inMaintenance){originalMaintenance(self);return;}
 if(callbackActive())return;   // a native action still owns its element
 inMaintenance=true;serviceGuarded();originalMaintenance(self);observe();inMaintenance=false;
}
// ---- install ----
static bool untouchedAt(uintptr_t p,const unsigned char* b,size_t n){return gamebridge::executable(p)&&readable(p,n)&&!std::memcmp((void*)p,b,n);}
static std::string available(){
 struct Check {uintptr_t rva;std::vector<unsigned char> b;};
 const Check calls[]={
  {rva::formElements,{0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6}},{rva::formClose,{0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xb0,0x48,0x89,0x5d,0xd0}},
  {rva::elementsCopy,{0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x8d,0x64,0x24,0xd8}},{rva::elementsFree,{0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x89,0xd6}},
  {rva::settingsTab,{0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xc8,0x8b,0x50,0x1c,0x83,0xe2,0x20,0x76,0x0c}},{rva::controlsTab,{0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xc8,0x8b,0x50,0x1c,0x83,0xe2,0x20,0x76,0x10}},
  {rva::formInvalidate,{0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x70,0xff,0xff,0xff,0x48,0x89,0x5d,0x98}},{rva::formShow,{0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x88,0x54,0x24,0x20}},
  {rva::formGeometry,{0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x8b,0x43,0x5c,0x83,0xe0,0x30}},{rva::optionsApply,{0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc0,0x48,0x89,0xcb}},
  {rva::optionsCancel,{0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0xe8}},{rva::optionsDefaults,{0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0x5d,0xe0}},
  {rva::checkboxHandler,{0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x44,0x89,0x44,0x24,0x20}}};
 for(const auto& c:calls)if(!gamebridge::operation(base+c.rva,c.b.data(),c.b.size()))return "a Settings form function did not match";
 if(!readable(base+rva::desktop,8)||!readable(base+rva::optionsForm,8)||!readable(base+rva::uiScale,4)||!basis(rva::optionsElements,9).data||!basis(rva::controlsElements,44).data)return "the Settings form tables are unreadable";
 ready=true;return {};
}
static const unsigned char maintenancePrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0x4d,0xf8};
// Two ways the tabs reach the screen. Without ExCamera, this runtime hooks the GUI maintenance step itself. A
// Settings owner that is already there (ExCamera, which hooks it at its own enable) can host these tabs instead:
// it calls settings_tabs_claim_host() before the runtime hooks, then draws and routes them with the hosted_*
// exports below. The hook is made only at the settled title menu, after every mod has enabled, and only on the
// game's own entry bytes, so the two never stack: an owner that neither hosts nor is absent leaves the tabs
// unavailable, and the log says so.
static std::atomic<bool> hostClaimed{false};static bool refused=false;
static bool installHook(){
 if(hooked)return true;if(!ready||hostClaimed)return false;
 if(!untouchedAt(base+rva::maintenance,maintenancePrefix,sizeof maintenancePrefix)){refused=true;return false;}
 const auto c=MH_CreateHook((void*)(base+rva::maintenance),(void*)maintenance,(void**)&originalMaintenance);
 if(c!=MH_OK&&c!=MH_ERROR_ALREADY_CREATED){refused=true;return false;}
 const auto s=MH_EnableHook((void*)(base+rva::maintenance));hooked=s==MH_OK||s==MH_ERROR_ENABLED;if(!hooked)refused=true;return hooked;
}
static bool anyTabs(){AcquireSRWLockShared(&tabsLock);const bool any=!tabs.empty();ReleaseSRWLockShared(&tabsLock);return any;}
// From the presentation pulse at a settled title menu: 1 hooked now, -1 refused now (once), 0 nothing to do.
static int poll(){
 if(hooked||refused||hostClaimed||!ready||!anyTabs())return 0;
 return installHook()?1:-1;
}
static bool claimHost(){if(hooked)return false;hostClaimed=true;return true;}
static bool addTab(const wchar_t* title,void* context,RowsFn rows,ChangeFn change,EventFn event){
 if(!title||!*title||wcslen(title)>24||!rows)return false;std::wstring t(title);const bool controls=t==L"@Controls";
 AcquireSRWLockExclusive(&tabsLock);bool ok=true;size_t pages=0;
 for(const auto& x:tabs){if(x.title==t&&x.context==context&&x.rows==rows)ok=false;if(!x.controls)++pages;}
 if(!controls&&pages>=maxTabs)ok=false;if(ok)tabs.push_back({t,context,rows,change,event,controls});
 ReleaseSRWLockExclusive(&tabsLock);
 rebuildWanted=true;return ok;
}
// ---- hosted access: another Settings owner draws these tabs (one snapshot per form it opens) ----
static std::vector<Tab> hostedTabs;
static int hostedOpen(){AcquireSRWLockShared(&tabsLock);hostedTabs=tabs;ReleaseSRWLockShared(&tabsLock);
 for(const auto& t:hostedTabs)tabEvent(t,0);return int(hostedTabs.size());}
static int hostedTitle(int tab,wchar_t* out,int room){
 if(tab<0||size_t(tab)>=hostedTabs.size()||!out||room<2)return -1;const auto& t=hostedTabs[size_t(tab)].title;
 const int n=(std::min)(int(t.size()),room-1);std::wmemcpy(out,t.c_str(),size_t(n));out[n]=0;return hostedTabs[size_t(tab)].controls?1:0;}
static int hostedRows(int tab,int page,Row* out,int room){
 if(tab<0||size_t(tab)>=hostedTabs.size()||!out||room<1)return 0;const auto rows=rowsOf(hostedTabs[size_t(tab)],page);
 const int n=(std::min)(int(rows.size()),room);for(int i=0;i<n;++i)out[i]=rows[size_t(i)];return n;}
static void hostedChange(int tab,int page,int id,int kind,int delta,float position,int checked,int key){
 if(tab>=0&&size_t(tab)<hostedTabs.size())tabChange(hostedTabs[size_t(tab)],page,id,kind,delta,position,checked,key);}
// tab -1 sends the event to every tab (opened/applied/cancelled/closed); Defaults goes to the tab shown.
static void hostedEvent(int tab,int event){
 for(size_t i=0;i<hostedTabs.size();++i)if(tab<0||size_t(tab)==i)tabEvent(hostedTabs[i],event);if(event==4&&tab<0)hostedTabs.clear();}
static bool removeTab(const wchar_t* title,void* context){
 if(!title)return false;AcquireSRWLockExclusive(&tabsLock);const auto n=tabs.size();
 tabs.erase(std::remove_if(tabs.begin(),tabs.end(),[&](const Tab& x){return x.title==title&&x.context==context;}),tabs.end());
 const bool found=tabs.size()!=n;ReleaseSRWLockExclusive(&tabsLock);
 // The open form keeps its copy until it closes; its callbacks may still run until then.
 return found;
}
}
