#pragma once
#include <deque>
// ui.sound(name[, volume]): one of the game's own single sounds (a Sound.rpk member such as "ReprSphereA1" or
// "CleanItem01"), played at the player the way the repair/clean use code does (static, 11C641..11C69E):
//   id = CC810(name)            load a single sound by exact name (an FPC string; the game caches the id)
//   CD130(*4EEC90, id, player + 0xAB4, volume 0.15, pitch 0.9 + 0.2 * rand, flags 0)
// CD130 checks its group is enabled (+0x0C & 3) and ORs the group's own flags (+0x68). Names are checked against
// Sound.rpk's member table first, so CC810 never sees a name the archive lacks. Requests queue from any callback
// and play on the next presentation pulse, the game-flow thread (as player.damage does).
namespace sounds {
using Load=int(*)(const char*);using Play=void(*)(uintptr_t group,int id,const float* position,float volume,float pitch,int flags);
static bool possible=false;static std::set<std::string> known;
struct Literal {int64_t references=-1,length=0;char text[24]{};};   // FPC constant string: never freed (the game may keep the pointer)
static std::deque<Literal> literals;static std::map<std::string,int> ids;
static std::mutex lock;static std::vector<std::pair<std::string,float>> pending;
// Members are stored lowercase ("reprspherea1"); the game's own literal is "ReprSphereA1". Names compare without case
// and the archive's spelling is what CC810 receives.
static std::string lower(std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;}
static void readIndex(const std::filesystem::path& rpk){
 std::ifstream in(rpk,std::ios::binary);unsigned char head[8]{};if(!in.read((char*)head,8)||head[0]!=1||head[1]!=0x0c||head[2]!=0xbf||head[3]!=0xaf)return;
 uint32_t length=0;std::memcpy(&length,head+4,4);if(length%32||length>(64u<<20))return;std::vector<char> table(length);if(!in.read(table.data(),length))return;
 for(uint32_t o=0;o<length;o+=32){const auto n=strnlen(table.data()+o,16);known.insert(lower(std::string(table.data()+o,n)));}
}
static void available(const std::filesystem::path& game){
 const unsigned char loadPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x4d,0xf8,0xc7,0x45,0xf4};
 const unsigned char playPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xb0,0x48,0x89,0x7d};
 if(!game.empty())readIndex(game/L"Sound.rpk");
 possible=!known.empty()&&gamebridge::operation(base+0xcc810,loadPrefix,sizeof loadPrefix)&&gamebridge::operation(base+0xcd130,playPrefix,sizeof playPrefix)&&readable(base+0x4eec90,8);
}
static bool validName(const std::string& n){if(n.empty()||n.size()>16)return false;for(char c:n)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return known.count(lower(n))!=0;}
// From Lua: false for a name Sound.rpk does not have (the caller reports it); at most 8 waiting.
static bool request(const std::string& name,float volume){
 if(!possible||!validName(name))return false;std::lock_guard<std::mutex> g(lock);if(pending.size()>=8)return false;pending.push_back({lower(name),volume});return true;
}
static int load(const char* text){__try{return ((Load)(base+0xcc810))(text);}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}}
static bool play(uintptr_t group,int id,const float* at,float volume,float pitch){__try{((Play)(base+0xcd130))(group,id,at,volume,pitch,0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void deliver(scripting::Host& target){
 decltype(pending) ready;{std::lock_guard<std::mutex> g(lock);ready.swap(pending);}if(ready.empty())return;
 uintptr_t group=0,player=0;
 if(!worldPulse||!copyWorldMemory(base+0x4eec90,&group,8)||!group||!copyWorldMemory(base+0x4f0830,&player,8)||!player||!readable(player+0xab4,12))return;
 for(const auto& [name,volume]:ready){
  auto found=ids.find(name);
  if(found==ids.end()){literals.emplace_back();auto& l=literals.back();l.length=int64_t(name.size());std::memcpy(l.text,name.data(),name.size());
   found=ids.emplace(name,load(l.text)).first;if(found->second<0)target.report("runtime","warn",("ui.sound: the game could not load "+name).c_str());}
  if(found->second<0)continue;
  const float pitch=0.9f+0.2f*float(std::rand()%1000)/1000.f;
  if(!play(group,found->second,(const float*)(player+0xab4),volume,pitch))target.report("runtime","warn","ui.sound: the game's sound call failed");
 }
}
}
