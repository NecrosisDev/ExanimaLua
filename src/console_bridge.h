#pragma once
#include <cwctype>
#include <deque>
#include <GL/gl.h>
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"gdi32.lib")
// In-game mod console: a scrolling list of warnings and errors drawn over the game's own frame, inside the
// runtime's existing present detour (0x21e60), on the game's render thread just before it presents. Sources:
// everything the runtime logs (Lua errors, addon log lines, the game's own script errors, runtime state) and
// any other mod's log file under mods/ and the EMTK folder, tailed by a small reader thread (files only; it never
// calls the game). The toggle key is read from ScriptRuntime.ini [console] key (a virtual-key code; 0 = unbound,
// the default). Open: Tab or a click on the header cycles All / Warnings / Errors; PgUp/PgDn/mouse wheel scroll.
// A corner badge counts errors that arrived while it was closed. Drawing saves and restores the GL state it
// touches and uses the fixed-function pipeline with GDI bitmap font lists, rebuilt when the context changes.
namespace console {
enum Level : uint8_t {info,warning,error};
struct Line {Level level=info;std::string source,text;};
static SRWLOCK lock=SRWLOCK_INIT;static std::deque<Line> lines;static constexpr size_t lineMax=600;
static std::atomic<unsigned> unseenErrors{0};static std::atomic<bool> open{false};
static int filter=0,scroll=0;static unsigned toggleKey=0;
static Level levelOf(const char* level){return !std::strcmp(level,"error")?error:!std::strcmp(level,"warn")?warning:info;}
// Machine-readable copy for agents and smoke tests (console/ beside ScriptRuntime.dll): console.jsonl, one JSON
// object per line {"ts","level","source","message"}, rotated to console.jsonl.1 at 2 MiB, each launch opening with a
// {"level":"session",...} line; console-summary.json, rewritten every two seconds with the counts since launch.
static std::vector<std::string> pendingJson;static uint64_t errorCount=0,warningCount=0,infoCount=0;static std::string lastError,lastWarning;
static std::string jsonText(const std::string& s){
 std::string out="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<0x20){char b[8];snprintf(b,sizeof b,"\\u%04x",c);out+=b;}else out+=char(c);}return out+"\"";
}
static std::string timestamp(){SYSTEMTIME t;GetSystemTime(&t);char b[32];snprintf(b,sizeof b,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return b;}
static void record(const char* source,Level level,const std::string& line){   // under lock
 pendingJson.push_back("{\"ts\":"+jsonText(timestamp())+",\"level\":\""+(level==error?"error":level==warning?"warning":"info")+"\",\"source\":"+jsonText(source)+",\"message\":"+jsonText(line)+"}");
 if(pendingJson.size()>4096)pendingJson.erase(pendingJson.begin());
 if(level==error){++errorCount;lastError=std::string(source)+": "+line;}else if(level==warning){++warningCount;lastWarning=std::string(source)+": "+line;}else ++infoCount;
}
static void add(const char* source,Level level,const std::string& text){
 if(text.empty())return;
 AcquireSRWLockExclusive(&lock);
 for(size_t s=0;s<text.size();){size_t e=text.find('\n',s);if(e==std::string::npos)e=text.size();if(e>s)record(source,level,text.substr(s,e-s));s=e+1;}
 size_t start=0;while(start<text.size()){size_t end=text.find('\n',start);if(end==std::string::npos)end=text.size();
  std::string part=text.substr(start,end-start);while(!part.empty()&&(part.back()=='\r'||part.back()==' '))part.pop_back();
  if(!part.empty()){lines.push_back({level,source,part});if(lines.size()>lineMax)lines.pop_front();}start=end+1;}
 ReleaseSRWLockExclusive(&lock);
 if(level==error&&!open.load())++unseenErrors;
}
// ui.notify: short messages that fade, bottom centre, shown whether or not the console is open (the game has no
// toast or notice line of its own). At most four at a time, each for about four seconds.
struct Toast {std::string text;ULONGLONG until=0;};
static std::deque<Toast> toasts;
static void notify(const std::string& message){
 if(message.empty())return;AcquireSRWLockExclusive(&lock);
 toasts.push_back({message.substr(0,160),GetTickCount64()+4000});while(toasts.size()>4)toasts.pop_front();ReleaseSRWLockExclusive(&lock);
}
// Called for every runtime log line (plugin.cpp runtimeLog).
static void report(const char* id,const char* level,const char* message){add(id&&*id?id:"runtime",levelOf(level?level:"info"),message?message:"");}
// ---- other mods' log files ----
static Level classify(const std::string& line){
 std::string low=line;for(auto& c:low)c=char(std::tolower((unsigned char)c));
 if(low.find("error")!=std::string::npos||low.find("fail")!=std::string::npos||low.find("exception")!=std::string::npos||low.find("fault")!=std::string::npos)return error;
 if(low.find("warn")!=std::string::npos||low.find("refused")!=std::string::npos||low.find("unavailable")!=std::string::npos)return warning;
 return info;
}
static bool logFile(const std::filesystem::path& p){
 std::wstring name=p.filename().wstring();for(auto& c:name)c=wchar_t(std::towlower(c));
 if(name==L"scriptruntime.log"||name==L"scriptruntime.log.previous")return false;   // already in-process
 return name.size()>4&&(name.compare(name.size()-4,4,L".log")==0||(name.find(L"log")!=std::wstring::npos&&name.compare(name.size()-4,4,L".txt")==0));
}
static std::vector<std::filesystem::path> roots;static std::map<std::wstring,uintmax_t> offsets;static std::atomic<bool> tailing{false};
static void tailOnce(bool first){
 std::error_code e;
 for(const auto& root:roots)for(auto it=std::filesystem::recursive_directory_iterator(root,std::filesystem::directory_options::skip_permission_denied,e);
  !e&&it!=std::filesystem::recursive_directory_iterator();it.increment(e)){
  if(it.depth()>3){it.disable_recursion_pending();continue;}
  if(!it->is_regular_file(e)||!logFile(it->path()))continue;
  const auto size=it->file_size(e);if(e)continue;const auto key=it->path().wstring();auto found=offsets.find(key);
  if(found==offsets.end()){offsets[key]=first?size:0;if(first)continue;found=offsets.find(key);}
  if(size<found->second)found->second=0;   // rotated or truncated
  if(size==found->second)continue;
  std::ifstream f(it->path(),std::ios::binary);if(!f)continue;f.seekg(std::streamoff(found->second));
  std::string chunk(size_t((std::min)(size-found->second,uintmax_t(65536))),'\0');f.read(chunk.data(),std::streamsize(chunk.size()));chunk.resize(size_t(f.gcount()));
  const size_t lastBreak=chunk.rfind('\n');if(lastBreak==std::string::npos)continue;chunk.resize(lastBreak+1);found->second+=chunk.size();
  const std::string source=it->path().parent_path().filename().string();
  size_t start=0;while(start<chunk.size()){size_t end=chunk.find('\n',start);std::string line=chunk.substr(start,end-start);start=end+1;
   const Level level=classify(line);if(level!=info)add(source.c_str(),level,line);}
 }
}
static std::filesystem::path outputDirectory;static ULONGLONG launched=0;
static void flush(){
 if(outputDirectory.empty())return;std::vector<std::string> out;uint64_t errors=0,warnings=0,infos=0;std::string err,warn;
 AcquireSRWLockExclusive(&lock);out.swap(pendingJson);errors=errorCount;warnings=warningCount;infos=infoCount;err=lastError;warn=lastWarning;ReleaseSRWLockExclusive(&lock);
 std::error_code e;std::filesystem::create_directories(outputDirectory,e);const auto log=outputDirectory/L"console.jsonl";
 if(!out.empty()){
  if(std::filesystem::exists(log,e)&&std::filesystem::file_size(log,e)>2*1024*1024){std::filesystem::rename(log,outputDirectory/L"console.jsonl.1",e);}
  std::ofstream f(log,std::ios::binary|std::ios::app);for(const auto& line:out)f<<line<<'\n';
 }
 const auto summary=outputDirectory/L"console-summary.json",temporary=outputDirectory/L"console-summary.json.tmp";
 {std::ofstream f(temporary,std::ios::binary|std::ios::trunc);
  f<<"{\"updated\":"<<jsonText(timestamp())<<",\"runtime\":"<<jsonText(scripting::runtimeVersion)<<",\"seconds_since_launch\":"<<(GetTickCount64()-launched)/1000
   <<",\"errors\":"<<errors<<",\"warnings\":"<<warnings<<",\"info\":"<<infos<<",\"last_error\":"<<jsonText(err)<<",\"last_warning\":"<<jsonText(warn)<<"}\n";}
 MoveFileExW(temporary.c_str(),summary.c_str(),MOVEFILE_REPLACE_EXISTING);
}
static DWORD WINAPI tailThread(void*){bool first=true;ULONGLONG next=0;while(tailing.load()){try{tailOnce(first);if(GetTickCount64()>=next){flush();next=GetTickCount64()+2000;}}catch(...){}first=false;Sleep(1000);}return 0;}
// The session line names the runtime and every mod folder present (EMTK enables a subset; each addon that loads
// also logs its own "loaded" line).
static void sessionStart(const std::filesystem::path& mods){
 std::string list;std::error_code e;
 for(const auto& d:std::filesystem::directory_iterator(mods,e)){if(!d.is_directory(e))continue;if(!list.empty())list+=",";list+=jsonText(d.path().filename().string());}
 AcquireSRWLockExclusive(&lock);
 pendingJson.push_back("{\"ts\":"+jsonText(timestamp())+",\"level\":\"session\",\"source\":\"runtime\",\"message\":\"session start\",\"runtime\":"+jsonText(scripting::runtimeVersion)+
  ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"mods\":["+list+"]}");
 ReleaseSRWLockExclusive(&lock);
}
static void startTailing(const std::filesystem::path& mods){
 if(tailing.exchange(true))return;launched=GetTickCount64();outputDirectory=mods/L"com.necrosis.script-runtime"/L"console";sessionStart(mods);roots.push_back(mods);
 std::error_code e;const auto emtk=mods.parent_path()/L".emtk";if(std::filesystem::is_directory(emtk,e))roots.push_back(emtk);
 if(HANDLE t=CreateThread(nullptr,0,tailThread,nullptr,0,nullptr))CloseHandle(t);else tailing=false;
}
// ---- settings ----
static void readSettings(const std::filesystem::path& ini){
 toggleKey=GetPrivateProfileIntW(L"console",L"key",0,ini.c_str());if(toggleKey>0xfe)toggleKey=0;
}
// ---- drawing (render thread, GL context current) ----
static HGLRC fontContext=nullptr;static GLuint fontBase=0;static int fontHeight=22,charWidth=12;static HFONT font=nullptr;
static bool keyWas[256]{};static bool clickWas=false;
static bool pressed(unsigned vk){const bool down=(GetAsyncKeyState(int(vk))&0x8000)!=0;const bool edge=down&&!keyWas[vk&0xff];keyWas[vk&0xff]=down;return edge;}
static void ensureFont(HDC dc){
 const HGLRC current=wglGetCurrentContext();if(!current)return;
 if(current==fontContext&&fontBase)return;
 if(!font)font=CreateFontW(-fontHeight,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"Consolas");
 const GLuint base=glGenLists(256);if(!base)return;
 const HGDIOBJ old=SelectObject(dc,font);const BOOL ok=wglUseFontBitmapsA(dc,0,256,base);
 SIZE m{};if(GetTextExtentPoint32W(dc,L"M",1,&m)&&m.cx>0)charWidth=m.cx;SelectObject(dc,old);
 if(!ok){glDeleteLists(base,256);return;}
 fontBase=base;fontContext=current;
}
static void text(int x,int y,const std::string& s){glRasterPos2i(x,y);glListBase(fontBase);glCallLists(GLsizei((std::min)(s.size(),size_t(220))),GL_UNSIGNED_BYTE,s.data());}
static void quad(int x0,int y0,int x1,int y1){glBegin(GL_QUADS);glVertex2i(x0,y0);glVertex2i(x1,y0);glVertex2i(x1,y1);glVertex2i(x0,y1);glEnd();}
static void color(Level l){if(l==error)glColor4f(1.f,.42f,.36f,1.f);else if(l==warning)glColor4f(.96f,.8f,.38f,1.f);else glColor4f(.86f,.82f,.74f,1.f);}
using UseProgram=void(APIENTRY*)(GLuint);
static void drawBody(HDC dc,HWND window){
 GLint view[4]{};glGetIntegerv(GL_VIEWPORT,view);const int w=view[2],h=view[3];if(w<200||h<200)return;
 ensureFont(dc);if(!fontBase)return;
 GLint program=0;glGetIntegerv(0x8B8D/*GL_CURRENT_PROGRAM*/,&program);static UseProgram use=(UseProgram)wglGetProcAddress("glUseProgram");
 if(use&&program)use(0);
 glPushAttrib(GL_ALL_ATTRIB_BITS);glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,w,h,0,-1,1);
 glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
 glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 const int row=fontHeight+3;
 {
  const ULONGLONG now=GetTickCount64();std::vector<Toast> live;
  AcquireSRWLockExclusive(&lock);while(!toasts.empty()&&toasts.front().until<=now)toasts.pop_front();live.assign(toasts.begin(),toasts.end());ReleaseSRWLockExclusive(&lock);
  int y=h-int(h*0.18)-int(live.size())*(row+8);
  for(const auto& t:live){const float a=(std::min)(1.f,float(t.until-now)/800.f);const int tw=int(t.text.size())*charWidth+24,x=(w-tw)/2;
   glColor4f(.06f,.05f,.04f,.8f*a);quad(x,y,x+tw,y+row+6);glColor4f(.94f,.88f,.74f,a);text(x+12,y+row-1,t.text);y+=row+8;}
 }
 if(!open.load()){
  const unsigned unseen=unseenErrors.load();
  if(unseen){const std::string badge=" "+std::to_string(unseen)+(unseen==1?" mod error ":" mod errors ");
   const int bw=int(badge.size())*charWidth+8;glColor4f(.35f,.05f,.04f,.85f);quad(w-bw-12,12,w-12,12+row+4);glColor4f(1.f,.55f,.45f,1.f);text(w-bw-8,12+row-2,badge);}
 }else{
  const int x0=24,y0=24,x1=w-24,y1=int(h*0.62);
  glColor4f(.06f,.05f,.04f,.86f);quad(x0,y0,x1,y1);glColor4f(.55f,.45f,.3f,.9f);quad(x0,y0+row+8,x1,y0+row+10);
  static const char* names[3]={"All","Warnings","Errors"};int bx=x0+12;
  POINT p{};GetCursorPos(&p);if(window)ScreenToClient(window,&p);const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,click=down&&!clickWas;clickWas=down;
  glColor4f(.92f,.86f,.72f,1.f);text(bx,y0+row,"MOD CONSOLE");bx+=charWidth*13;
  for(int i=0;i<3;++i){const std::string label=std::string("[")+names[i]+"]";const int bw=int(label.size())*charWidth;
   if(click&&p.x>=bx&&p.x<bx+bw&&p.y>=y0&&p.y<y0+row+8){filter=i;scroll=0;}
   if(filter==i)glColor4f(1.f,.85f,.45f,1.f);else glColor4f(.6f,.56f,.5f,1.f);text(bx,y0+row,label);bx+=bw+16;}
  glColor4f(.6f,.56f,.5f,1.f);text(bx+8,y0+row,"Tab: filter  PgUp/PgDn: scroll");
  std::vector<Line> shown;AcquireSRWLockShared(&lock);
  for(const auto& l:lines)if(filter==0||(filter==1&&l.level>=warning)||(filter==2&&l.level==error))shown.push_back(l);ReleaseSRWLockShared(&lock);
  const int capacity=(y1-(y0+row+16))/row;const int maxScroll=(std::max)(0,int(shown.size())-capacity);scroll=(std::min)(scroll,maxScroll);
  const int first=(std::max)(0,int(shown.size())-capacity-scroll);int y=y0+row*2+12;
  for(int i=first;i<int(shown.size())&&i<first+capacity;++i){color(shown[size_t(i)].level);text(x0+12,y,"["+shown[size_t(i)].source+"] "+shown[size_t(i)].text);y+=row;}
  if(shown.empty()){glColor4f(.6f,.56f,.5f,1.f);text(x0+12,y,filter==0?"Nothing logged yet.":"Nothing at this level.");}
 }
 glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glPopAttrib();
 if(use&&program)use(GLuint(program));
}
static void inputs(){
 if(toggleKey&&GetForegroundWindow()&&pressed(toggleKey)){const bool now=!open.load();open=now;if(now){unseenErrors=0;scroll=0;}}
 if(!open.load())return;
 if(pressed(VK_TAB)){filter=(filter+1)%3;scroll=0;}
 if(pressed(VK_PRIOR))scroll+=10;if(pressed(VK_NEXT))scroll=(std::max)(0,scroll-10);
}
// Called before the game presents, with its device context; never throws into the game.
static void draw(HDC dc){
 __try{
  const HWND window=WindowFromDC(dc);if(!window||GetWindowThreadProcessId(window,nullptr)!=GetCurrentThreadId())return;
  inputs();
  bool anyToast=false;AcquireSRWLockShared(&lock);anyToast=!toasts.empty();ReleaseSRWLockShared(&lock);
  if(!open.load()&&!unseenErrors.load()&&!anyToast)return;drawBody(dc,window);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
