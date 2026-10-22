#pragma once
#include <deque>
#include <unordered_set>
// The "Mod Settings" tab of the game's Settings form (settings_tabs.h draws it). One place for every mod's options:
//  - Lua addons declare rows with settings.define and read them with settings.get / settings.changed;
//  - native EMTK plugins' config.toml [[setting]] entries (true/false ones are editable; EMTK reads them at
//    startup, so a change applies at the next launch; other types are shown read-only);
//  - the runtime's own options (the console key).
// Values live in named profiles, mods/<runtime>/profiles/<name>.ini (UTF-8, [owner] key=value); the active one is
// ScriptRuntime.ini [settings] profile. Edits are a draft until Apply, as for the game's own pages; Cancel drops them.
namespace modsettings {
using scripting::SettingDef;using scripting::SettingChange;
struct Section {std::string owner,title;std::vector<SettingDef> defs;std::filesystem::path config;bool native=false;std::string category;bool categoryRead=false;};
using Values=std::map<std::string,std::map<std::string,std::string>>;   // owner -> key -> value text
static std::recursive_mutex lock;
static std::vector<Section> sections;static Values committed,staged;
static std::string activeProfile="Default",stagedProfile;static bool editing=false;
static std::filesystem::path profilesDirectory,settingsIni;
static std::vector<std::pair<std::string,std::vector<SettingChange>>> pending;   // applied changes for Lua owners
static bool registered=false;
static std::wstring wide(const std::string& s){
 if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
 std::wstring w(size_t((std::max)(n,0)),L'\0');if(n>0)MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;
}
static std::string narrow(const std::wstring& w){
 if(w.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
 std::string s(size_t((std::max)(n,0)),'\0');if(n>0)WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),s.data(),n,nullptr,nullptr);return s;
}
static std::string trim(std::string s){while(!s.empty()&&(s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;return s.substr(i);}
// ---- profile files ----
static bool validProfile(const std::string& n){if(n.empty()||n.size()>24)return false;for(char c:n)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='-'||c=='_'))return false;return true;}
static Values readProfile(const std::string& name){
 Values v;if(profilesDirectory.empty()||!validProfile(name))return v;std::ifstream in(profilesDirectory/wide(name+".ini"),std::ios::binary);std::string line,owner;
 while(std::getline(in,line)){line=trim(line);if(line.empty()||line[0]==';'||line[0]=='#')continue;
  if(line.front()=='['&&line.back()==']'){owner=line.substr(1,line.size()-2);continue;}
  const auto eq=line.find('=');if(eq==std::string::npos||owner.empty())continue;v[owner][trim(line.substr(0,eq))]=trim(line.substr(eq+1));}
 return v;
}
static bool writeProfile(const std::string& name,const Values& v){
 if(profilesDirectory.empty()||!validProfile(name))return false;std::error_code e;std::filesystem::create_directories(profilesDirectory,e);
 const auto path=profilesDirectory/wide(name+".ini"),temp=profilesDirectory/wide(name+".ini.tmp");
 {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)return false;
  out<<"; ExaniMate Lua runtime Mod Settings profile. Edited in game: Settings > Mod Settings.\r\n";
  for(const auto& [owner,keys]:v){if(keys.empty())continue;out<<"\r\n["<<owner<<"]\r\n";for(const auto& [k,x]:keys)out<<k<<"="<<x<<"\r\n";}
  if(!out.flush())return false;}
 return MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
static std::vector<std::string> profiles(){
 std::vector<std::string> names{"Default"};std::error_code e;
 if(!profilesDirectory.empty())for(const auto& f:std::filesystem::directory_iterator(profilesDirectory,e)){
  if(f.path().extension()!=L".ini")continue;const auto n=narrow(f.path().stem().wstring());if(validProfile(n)&&n!="Default")names.push_back(n);}
 std::sort(names.begin()+1,names.end());return names;
}
// ---- native config.toml [[setting]] entries ----
struct NativeEntry {std::string id,name,fallback,value;};
static std::vector<NativeEntry> nativeEntries(const std::filesystem::path& toml,std::string& plugin){
 std::ifstream in(toml,std::ios::binary);std::string line;std::vector<NativeEntry> out;bool inSetting=false,inPlugin=false;
 auto unquote=[](std::string v){v=trim(v);if(v.size()>=2&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);return v;};
 while(std::getline(in,line)){const auto t=trim(line);
  if(t=="[[setting]]"){out.push_back({});inSetting=true;inPlugin=false;continue;}
  if(!t.empty()&&t[0]=='['){inSetting=false;inPlugin=t=="[plugin]";continue;}
  const auto eq=t.find('=');if(eq==std::string::npos)continue;const auto k=trim(t.substr(0,eq)),v=trim(t.substr(eq+1));
  if(inPlugin&&k=="name")plugin=unquote(v);
  if(!inSetting)continue;auto& e=out.back();
  if(k=="id")e.id=unquote(v);else if(k=="name")e.name=unquote(v);else if(k=="default")e.fallback=v;else if(k=="value")e.value=v;}
 out.erase(std::remove_if(out.begin(),out.end(),[](const NativeEntry& e){return e.id.empty();}),out.end());return out;
}
// Writes `value = ...` into one [[setting]] block, keeping every other line as it was.
static bool writeNativeValue(const std::filesystem::path& toml,const std::string& id,const std::string& value){
 std::ifstream in(toml,std::ios::binary);if(!in)return false;std::vector<std::string> lines;std::string line;while(std::getline(in,line))lines.push_back(line);in.close();
 bool inSetting=false,match=false,done=false;size_t insertAt=std::string::npos;
 for(size_t i=0;i<=lines.size()&&!done;++i){
  const bool end=i==lines.size();const auto t=end?std::string("["):trim(lines[i]);
  if(!t.empty()&&t[0]=='['){if(match&&insertAt!=std::string::npos){lines.insert(lines.begin()+std::ptrdiff_t(insertAt),"value = "+value);done=true;break;}
   inSetting=t=="[[setting]]";match=false;insertAt=std::string::npos;continue;}
  if(!inSetting)continue;const auto eq=t.find('=');if(eq==std::string::npos)continue;const auto k=trim(t.substr(0,eq));
  if(k=="id"){std::string v=trim(t.substr(eq+1));if(v.size()>=2&&v.front()=='"')v=v.substr(1,v.size()-2);match=v==id;insertAt=i+1;}
  else if(match&&k=="value"){lines[i]="value = "+value;done=true;}
 }
 if(!done)return false;const auto temp=toml;auto tmp=temp;tmp+=L".tmp";
 {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);if(!out)return false;for(const auto& l:lines)out<<l<<"\n";if(!out.flush())return false;}
 return MoveFileExW(tmp.c_str(),toml.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
// ---- values ----
static const Section* section(const std::string& owner){for(const auto& s:sections)if(s.owner==owner)return &s;return nullptr;}
static std::string effective(const Values& v,const Section& s,const SettingDef& d){
 const auto o=v.find(s.owner);if(o!=v.end()){const auto k=o->second.find(d.key);if(k!=o->second.end())return k->second;}return d.fallback;
}
static void applyRuntime(const Values& v){const auto* s=section("runtime");if(!s)return;
 for(const auto& d:s->defs){if(d.key=="console_key"){const int k=std::atoi(effective(v,*s,d).c_str());console::toggleKey=unsigned(k>0&&k<255?k:0);}
  if(d.key=="profiler")scripting::Host::profiling=effective(v,*s,d)!="false";}}
static bool readSetting(const std::string& owner,const std::string& key,std::string& value){
 std::lock_guard<std::recursive_mutex> g(lock);const auto* s=section(owner);if(!s)return false;
 for(const auto& d:s->defs)if(d.key==key){value=effective(committed,*s,d);return true;}return false;
}
static bool defineSettings(const std::string& owner,const std::string& title,const std::vector<SettingDef>& defs){
 std::lock_guard<std::recursive_mutex> g(lock);if(!registered)return false;
 for(auto& s:sections)if(s.owner==owner){s.title=title;s.defs=defs;settingstabs::rebuildWanted=true;return true;}
 if(sections.size()>=48)return false;sections.push_back({owner,title,defs,{},false});settingstabs::rebuildWanted=true;return true;
}
// ---- the tab ----
struct Ref {int what=0;size_t section=0,def=0;};   // what: 0 profile, 1 new profile, 2 a setting
static std::vector<Ref> refs;
// Row texts are interned for the life of the process: a Settings host (ExCamera) may still hold a row's label or
// value pointer after the next rows() call, so no text it was given is ever freed (distinct texts are bounded by the
// rows' labels and their values' steps).
static std::unordered_set<std::wstring> texts;
static const wchar_t* keep(std::wstring s){return texts.insert(std::move(s)).first->c_str();}
static std::wstring keyName(int vk){
 if(vk<=0)return L"Unbound";UINT scan=MapVirtualKeyW(UINT(vk),MAPVK_VK_TO_VSC);LONG l=LONG(scan<<16);
 if((vk>=VK_PRIOR&&vk<=VK_HELP)||vk==VK_DIVIDE||vk==VK_NUMLOCK||vk==VK_RCONTROL||vk==VK_RMENU)l|=1<<24;
 wchar_t b[32]{};if(scan&&GetKeyNameTextW(l,b,32))return b;return L"Key "+std::to_wstring(vk);
}
static std::wstring numberText(double v,bool percent=false){wchar_t b[24];if(percent)swprintf(b,24,L"%.4g %%",v*100.0);else swprintf(b,24,L"%.4g",v);return b;}
static int rows(void*,int page,settingstabs::Row* out,int room){
 std::lock_guard<std::recursive_mutex> g(lock);refs.clear();if(page!=0)return 0;
 const Values& v=editing?staged:committed;const std::string& profile=editing?stagedProfile:activeProfile;int n=0;
 auto push=[&](settingstabs::Row r,Ref ref){if(n>=room)return;r.id=int(refs.size());refs.push_back(ref);out[n++]=r;};
 settingstabs::Row r;r.kind=0;r.label=L"Profile";push(r,{});refs.back().what=-2;
 r={};r.kind=3;r.label=L"Profile";r.value=keep(wide(profile));push(r,{0});
 r={};r.kind=3;r.label=L"New profile";r.value=L"Copy";push(r,{1});
 for(size_t s=0;s<sections.size()&&n<room;++s){const auto& sec=sections[s];if(sec.defs.empty())continue;
  r={};r.kind=0;r.label=keep(wide(sec.title));push(r,{-3,s});
  for(size_t i=0;i<sec.defs.size()&&n<room;++i){const auto& d=sec.defs[i];const auto value=effective(v,sec,d);r={};
   std::wstring label=wide(d.label);if(sec.native&&d.type!="info"&&d.type!="header")label+=L" *";
   if(d.type=="header"){r.kind=5;r.inactive=1;r.value=L"";}   // a sub-header: a dimmed text line with no value
   else if(d.type=="toggle"){r.kind=1;r.checked=value=="true";}
   else if(d.type=="number"){r.kind=2;const double x=std::strtod(value.c_str(),nullptr);r.steps=int(std::lround((d.maximum-d.minimum)/d.step));
    r.position=float(std::clamp((x-d.minimum)/(d.maximum-d.minimum),0.0,1.0));r.value=keep(numberText(x,d.percent));}
   else if(d.type=="choice"){r.kind=3;r.value=keep(wide(value));}
   else if(d.type=="key"){r.kind=4;r.value=keep(keyName(std::atoi(value.c_str())));}
   else{r.kind=5;r.value=keep(wide(value));}
   r.label=keep(label);push(r,{2,s,i});}
 }
 bool natives=false;for(const auto& s:sections)natives=natives||s.native;
 if(natives&&n<room){r={};r.kind=5;r.label=L"* after a restart";push(r,{-1});}
 return n;
}
// A section's category for a host's grouping: `category = "..."` in its mod's manifest.toml (Graphics, Camera,
// Gameplay, Content or Utility); the runtime's own section and the Profile row are Utility. Read once per section.
static std::string categoryOf(Section& s){
 if(s.categoryRead)return s.category;s.categoryRead=true;
 if(s.owner=="runtime"){s.category="Utility";return s.category;}
 if(profilesDirectory.empty())return s.category;
 const auto mods=profilesDirectory.parent_path().parent_path();
 const std::string folder=s.owner.rfind("native:",0)==0?s.owner.substr(7):s.owner;
 std::ifstream in(mods/wide(folder)/L"manifest.toml",std::ios::binary);std::string line;
 while(std::getline(in,line)){line=trim(line);if(line.rfind("category",0)!=0)continue;const auto eq=line.find('=');if(eq==std::string::npos)continue;
  std::string v=trim(line.substr(eq+1));if(v.size()>=2&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);
  for(const char* known:{"Graphics","Camera","Gameplay","Content","Utility"})if(v==known){s.category=v;break;}break;}
 return s.category;
}
static int category(int id,wchar_t* out,int capacity){
 std::lock_guard<std::recursive_mutex> g(lock);if(id<0||size_t(id)>=refs.size())return 0;const auto ref=refs[size_t(id)];std::wstring text;
 if(ref.what==-2)text=L"Utility";else if(ref.what==-3&&ref.section<sections.size())text=wide(categoryOf(sections[ref.section]));
 if(out&&capacity>0){const size_t n=(std::min)(text.size(),size_t(capacity-1));std::wmemcpy(out,text.data(),n);out[n]=0;}
 return int(text.size());
}
// A row's help text (settings.define `help`), for a host that shows it as the row's tooltip. Returns its length in
// UTF-16 units (0 when the row has none), copying at most capacity-1 of them and a terminator.
static int help(int id,wchar_t* out,int capacity){
 std::lock_guard<std::recursive_mutex> g(lock);if(id<0||size_t(id)>=refs.size())return 0;const auto& ref=refs[size_t(id)];
 std::wstring text;
 if(ref.what==0)text=L"The set of values in use; click for the next one";else if(ref.what==1)text=L"A new profile starting from these values";
 else if(ref.what==2&&ref.section<sections.size()&&ref.def<sections[ref.section].defs.size())text=wide(sections[ref.section].defs[ref.def].help);
 if(sections.size()>ref.section&&ref.what==2&&sections[ref.section].native)text+=(text.empty()?L"":L" ")+std::wstring(L"(applies after a restart)");
 if(out&&capacity>0){const size_t n=(std::min)(text.size(),size_t(capacity-1));std::wmemcpy(out,text.data(),n);out[n]=0;}
 return int(text.size());
}
static void change(void*,int page,int id,int kind,int delta,float position,int checked,int key){
 std::lock_guard<std::recursive_mutex> g(lock);if(page!=0||!editing||id<0||size_t(id)>=refs.size())return;const auto ref=refs[size_t(id)];
 if(ref.what==0&&kind==3){const auto names=profiles();auto at=std::find(names.begin(),names.end(),stagedProfile);
  const size_t next=at==names.end()?0:(size_t(at-names.begin())+size_t((std::max)(delta,1)))%names.size();
  stagedProfile=names[next];staged=readProfile(stagedProfile);return;}
 if(ref.what==1&&kind==3){const auto names=profiles();for(int i=2;i<100;++i){const auto n="Profile "+std::to_string(i);
  if(std::find(names.begin(),names.end(),n)==names.end()&&n!=stagedProfile){stagedProfile=n;return;}}return;}
 if(ref.what!=2||ref.section>=sections.size()||ref.def>=sections[ref.section].defs.size())return;
 const auto& s=sections[ref.section];const auto& d=s.defs[ref.def];auto& slot=staged[s.owner][d.key];
 if(d.type=="toggle"&&kind==1)slot=checked?"true":"false";
 else if(d.type=="number"&&kind==2){const int steps=(std::max)(1,int(std::lround((d.maximum-d.minimum)/d.step)));
  const double x=std::clamp(d.minimum+std::lround(double(std::clamp(position,0.f,1.f))*steps)*d.step,d.minimum,d.maximum);char b[32];snprintf(b,sizeof b,"%.9g",x);slot=b;}
 else if(d.type=="choice"&&kind==3){auto at=std::find(d.choices.begin(),d.choices.end(),effective(staged,s,d));
  slot=d.choices[at==d.choices.end()?0:(size_t(at-d.choices.begin())+1)%d.choices.size()];}
 else if(d.type=="key"&&kind==4)slot=std::to_string(key>0&&key<255?key:0);
}
static void apply(){
 Values before=committed,after=staged;const std::string profile=stagedProfile;
 for(const auto& s:sections){std::vector<SettingChange> changes;
  for(const auto& d:s.defs){const auto old=effective(before,s,d),now=effective(after,s,d);if(old==now)continue;
   if(s.native){if(!writeNativeValue(s.config,d.key,now))console::report("runtime","warn",("Mod Settings: could not write "+d.key+" to "+narrow(s.config.wstring())).c_str());}
   else if(s.owner!="runtime")changes.push_back({d.key,now});}
  if(!changes.empty())pending.push_back({s.owner,std::move(changes)});}
 committed=std::move(after);activeProfile=profile;applyRuntime(committed);
 if(!writeProfile(activeProfile,committed))console::report("runtime","warn","Mod Settings: the profile file could not be written");
 if(!settingsIni.empty())WritePrivateProfileStringW(L"settings",L"profile",wide(activeProfile).c_str(),settingsIni.c_str());
}
static void event(void*,int e){
 std::lock_guard<std::recursive_mutex> g(lock);
 if(e==0){staged=committed;stagedProfile=activeProfile;editing=true;}
 else if(e==1){if(editing)apply();editing=false;}
 else if(e==2||e==4)editing=false;
 else if(e==3&&editing){for(const auto& s:sections)staged.erase(s.owner);}
}
// The pulse hands applied changes to the owning addons (settings.changed).
static void deliver(scripting::Host& target){
 decltype(pending) ready;{std::lock_guard<std::recursive_mutex> g(lock);ready.swap(pending);}
 for(const auto& [owner,changes]:ready)target.notifySettings(owner,changes);
}
static void start(const std::filesystem::path& control){
 std::lock_guard<std::recursive_mutex> g(lock);if(registered||control.empty())return;
 profilesDirectory=control/L"profiles";settingsIni=control/L"ScriptRuntime.ini";
 wchar_t name[64]{};GetPrivateProfileStringW(L"settings",L"profile",L"Default",name,64,settingsIni.c_str());
 activeProfile=validProfile(narrow(name))?narrow(name):"Default";committed=readProfile(activeProfile);
 // The console key's default is the ini's own [console] key (0 = unbound until the player picks one).
 SettingDef key;key.key="console_key";key.label="Console key";key.help="Opens the console of mod messages and errors";key.type="key";key.fallback=std::to_string(console::toggleKey);
 SettingDef profiler;profiler.key="profiler";profiler.label="Profiler";profiler.help="Logs each Lua mod's cost to the runtime's profiler folder every 30 seconds";profiler.type="toggle";profiler.fallback="true";
 sections.push_back({"runtime","Lua runtime",{key,profiler},{},false});
 std::error_code e;const auto mods=control.parent_path();
 for(const auto& folder:std::filesystem::directory_iterator(mods,e)){const auto toml=folder.path()/L"config.toml";if(!folder.is_directory(e)||!std::filesystem::is_regular_file(toml,e))continue;
  std::string plugin;const auto entries=nativeEntries(toml,plugin);if(entries.empty())continue;Section s;s.owner="native:"+narrow(folder.path().filename().wstring());
  s.title=plugin.empty()?narrow(folder.path().filename().wstring()):plugin;s.config=toml;s.native=true;
  for(const auto& x:entries){SettingDef d;d.key=x.id;d.label=x.name.empty()?x.id:x.name;const auto current=x.value.empty()?x.fallback:x.value;
   d.type=(x.fallback=="true"||x.fallback=="false")?"toggle":"info";d.fallback=current;s.defs.push_back(d);}
  if(sections.size()<48)sections.push_back(std::move(s));}
 applyRuntime(committed);
 registered=settingstabs::addTab(L"Mod Settings",nullptr,rows,change,event);
}
}
