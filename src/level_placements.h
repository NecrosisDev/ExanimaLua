#pragma once
// levels.add_placement: loose items added to a level as the game reads its .rfc, so a mod never ships a whole level
// file (two mods that did replaced each other's, and one replaced the installed level with another game build's).
// The member read (member_reads_bridge.h) hands every read level to `apply` after it is filled, by whichever
// provider or the game; this appends item placements to its items chunk and nothing else.
// Level layout: magic 0x3D23AFCF, then
// chunks {u32 tag, u32 size, bytes}. Items 0xDBC0 holds sub-chunks; placements 0xDBCE is a u32 count, that many
// chunks 0xDB01 {u32 item, u32, u32, f32 rot[9], f32 pos[3], tail}, then trailing bytes. A new row copies the first
// childless row (68 bytes, or with the empty child block every shipped loose item carries) with the item, an
// upright yaw rotation and the position replaced. Like any level edit, it shows in levels the game generates fresh,
// not in a saved game's copy of a level.
namespace placements {
struct Placement {std::string owner,level;uint32_t item=0;float pos[3]{};float yaw=0;};
constexpr uint32_t magic=0x3D23AFCF,itemsTag=0xDBC0,placesTag=0xDBCE,placeTag=0xDB01;constexpr size_t placeHead=68;
static const unsigned char noChildren[16]={0x00,0xec,0xe0,0x3d,0x08,0,0,0,0,0,0,0,0,0,0,0};
inline uint32_t u32(const std::string& b,size_t at){uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
inline void put32(std::string& b,size_t at,uint32_t v){std::memcpy(&b[at],&v,4);}
// Appends `rows` to the level in `bytes`. Returns false (bytes unchanged) when the layout is not as expected; a
// placement already present (same item within 1 cm) is skipped, so a level that already has it gets no twin.
inline bool append(std::string& bytes,const std::vector<Placement>& rows,size_t& added){
 added=0;if(rows.empty())return true;if(bytes.size()<12||u32(bytes,0)!=magic)return false;
 size_t p=4;
 while(p+8<=bytes.size()){
  const uint32_t tag=u32(bytes,p),size=u32(bytes,p+4);if(size>bytes.size()-p-8)return false;
  if(tag!=itemsTag){p+=8+size;continue;}
  const size_t itemsAt=p,itemsEnd=p+8+size;size_t q=p+8;
  while(q+8<=itemsEnd){
   const uint32_t sub=u32(bytes,q),subSize=u32(bytes,q+4);if(subSize>itemsEnd-q-8)return false;
   if(sub!=placesTag){q+=8+subSize;continue;}
   const size_t placesAt=q,placesEnd=q+8+subSize;if(subSize<4)return false;
   const uint32_t count=u32(bytes,q+8);size_t r=q+12;std::string templ;std::vector<std::array<float,4>> present;
   for(uint32_t i=0;i<count;++i){
    if(r+8>placesEnd)return false;const uint32_t rowTag=u32(bytes,r),rowSize=u32(bytes,r+4);if(rowSize>placesEnd-r-8||rowTag!=placeTag||rowSize<placeHead)return false;
    std::array<float,4> seen{};std::memcpy(&seen[0],bytes.data()+r+8+48,12);uint32_t id=u32(bytes,r+8);std::memcpy(&seen[3],&id,4);present.push_back(seen);
    const size_t tail=rowSize-placeHead;
    if(templ.empty()&&(tail==0||(tail==16&&!std::memcmp(bytes.data()+r+8+placeHead,noChildren,16))))templ=bytes.substr(r+8,rowSize);
    r+=8+rowSize;
   }
   if(templ.empty())return false;
   std::string extra;
   for(const auto& row:rows){
    bool twin=false;for(const auto& s:present){uint32_t id;std::memcpy(&id,&s[3],4);const float dx=s[0]-row.pos[0],dy=s[1]-row.pos[1],dz=s[2]-row.pos[2];
     if(id==row.item&&dx*dx+dy*dy+dz*dz<1.f)twin=true;}
    if(twin)continue;
    std::string b=templ;put32(b,0,row.item);const float c=std::cos(row.yaw),s=std::sin(row.yaw);const float rot[9]={c,0,s,0,1,0,-s,0,c};
    std::memcpy(&b[12],rot,36);std::memcpy(&b[48],row.pos,12);
    std::string chunk(8,'\0');put32(chunk,0,placeTag);put32(chunk,4,uint32_t(b.size()));extra+=chunk+b;++added;
    std::array<float,4> now{row.pos[0],row.pos[1],row.pos[2],0};std::memcpy(&now[3],&row.item,4);present.push_back(now);
   }
   if(!added)return true;
   const size_t grow=extra.size();if(grow>0x7fffffff-bytes.size())return false;
   bytes.insert(r,extra);put32(bytes,placesAt+8,count+uint32_t(added));put32(bytes,placesAt+4,uint32_t(subSize+grow));put32(bytes,itemsAt+4,uint32_t(size+grow));
   return true;
  }
  return false;   // an items chunk without placements
 }
 return false;
}
// Registrations from Lua (owner = addon id); an immutable snapshot is swapped on change so the loader thread reads
// it without a lock held while it edits.
static SRWLOCK lock=SRWLOCK_INIT;static std::shared_ptr<const std::vector<Placement>> all=std::make_shared<std::vector<Placement>>();
static std::atomic<uint64_t> applied{0},refused{0};
static std::string lower(std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;}
static bool add(const Placement& p){
 AcquireSRWLockExclusive(&lock);auto next=std::make_shared<std::vector<Placement>>(*all);size_t mine=0;
 const auto level=lower(p.level);bool same=false;
 // A reload declares its placements again: an identical one is kept once.
 for(const auto& x:*next){if(x.owner==p.owner)++mine;same=same||(x.owner==p.owner&&x.level==level&&x.item==p.item&&!std::memcmp(x.pos,p.pos,12)&&x.yaw==p.yaw);}
 const bool ok=same||(mine<64&&next->size()<512);
 if(ok&&!same){next->push_back(p);next->back().level=level;all=next;}ReleaseSRWLockExclusive(&lock);return ok;
}
static void forget(const std::string& owner){
 AcquireSRWLockExclusive(&lock);auto next=std::make_shared<std::vector<Placement>>(*all);
 next->erase(std::remove_if(next->begin(),next->end(),[&](const Placement& x){return x.owner==owner;}),next->end());all=next;ReleaseSRWLockExclusive(&lock);
}
static std::shared_ptr<const std::vector<Placement>> snapshot(){AcquireSRWLockShared(&lock);auto s=all;ReleaseSRWLockShared(&lock);return s;}
// From the member read, for every member: the placements for `name16` (a level member such as "exanima01.rfc").
static bool wants(const char* name16){
 const auto s=snapshot();if(s->empty())return false;const auto n=lower(name16);for(const auto& p:*s)if(p.level==n)return true;return false;
}
static bool transform(const char* name16,std::string& bytes){
 const auto s=snapshot();const auto n=lower(name16);std::vector<Placement> rows;for(const auto& p:*s)if(p.level==n)rows.push_back(p);
 size_t added=0;if(!append(bytes,rows,added)){++refused;return false;}if(added)++applied;return added!=0;
}
}
