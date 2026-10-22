#pragma once
// [script.records]: item database records and display strings merged into the game's own objdb.rdb and
// objstrings.rdb as the game reads them, so several mods can each add items without shipping whole databases (the
// last one would replace the others'). Each mod declares the ranges it owns in its manifest:
//   [script.records]
//   ids = [0x590, 0x59f]            # objdb record ids this mod adds
//   strings = [1201, 1232]          # objstrings indices (1-based, as records name them) this mod's strings take
//   objdb = "records/objdb.rdb"     # an .rdb holding only its new records and the game records it changes
//   objstrings = "records/objstrings.rdb"   # a strings package holding only its strings, in index order
// Ranges may not overlap another enabled mod's or the game's own records. A record inside the mod's id range is
// added; a game record the mod changes replaces the game's, except drop tables (chunk 0x200), where the rows the mod
// adds are appended to whatever the game and other mods have, so two mods can both add loot to one table. Strings
// are placed at their declared indices (the gap after the game's own strings is filled with empty entries), so a
// record's display-string index never needs rewriting. The merge is the member-read edit stage
// (member_reads_bridge.h), applied to whatever was read (the game's file, or a whole file another mod still ships).
// File formats (slots {u32 id, u32 flags, u32 offset, u32 size} after a u32 magic and a u32
// table length; records are tagged chunks whose size counts their header; strings: magic 02 0C BF AF, the same
// slot table {u32 a, u32 b, u32 offset, u32 size} and a text pool).
namespace recordmerge {
struct Mod {std::string id;uint32_t firstId=0,lastId=0,firstString=0,lastString=0;std::string db,strings;};
static SRWLOCK lock=SRWLOCK_INIT;static std::vector<Mod> mods;static std::atomic<bool> any{false};
inline uint32_t rd(const std::string& b,size_t at){uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
inline void wr(std::string& b,size_t at,uint32_t v){std::memcpy(&b[at],&v,4);}
struct Slot {uint32_t id,flags,offset,size;};
// An .rdb's slot table and data block; false when it is not one.
inline bool table(const std::string& b,std::vector<Slot>& slots,size_t& dataAt){
 if(b.size()<8)return false;const uint32_t magic=rd(b,0),length=rd(b,4);
 if((magic&0xFF00FF00u)!=0xDB000D00u||length%16||8ull+length>b.size())return false;
 dataAt=8+length;slots.resize(length/16);for(size_t i=0;i<slots.size();++i){std::memcpy(&slots[i],b.data()+8+16*i,16);}return true;
}
inline bool live(const Slot& s,const std::string& b,size_t dataAt){return s.size&&dataAt+uint64_t(s.offset)+s.size<=b.size();}
inline std::string recordOf(const Slot& s,const std::string& b,size_t dataAt){return b.substr(dataAt+s.offset,s.size);}
// Drop-table rows of a record (chunk 0x200: a 32-byte header row, then 16-byte rows); empty when none.
inline bool dropChunk(const std::string& r,size_t& at,size_t& size){
 for(size_t p=0;p+8<=r.size();){const uint32_t tag=rd(r,p),s=rd(r,p+4);if(s<8||p+s>r.size())return false;if(tag==0x200){at=p;size=s;return s>=8+32;}p+=s;}
 return false;
}
inline std::vector<std::string> dropRows(const std::string& r){
 std::vector<std::string> rows;size_t at=0,size=0;if(!dropChunk(r,at,size))return rows;
 for(size_t p=at+8+32;p+16<=at+size;p+=16)rows.push_back(r.substr(p,16));return rows;
}
// `merged` gains the rows of `mine` that `base` lacks (compared whole); the chunk size and the row count follow.
inline bool appendDrops(std::string& merged,const std::string& base,const std::string& mine){
 const auto had=dropRows(base),wanted=dropRows(mine),now=dropRows(merged);size_t at=0,size=0;if(!dropChunk(merged,at,size))return false;
 std::string extra;size_t rows=now.size();
 for(const auto& row:wanted)if(std::find(had.begin(),had.end(),row)==had.end()&&std::find(now.begin(),now.end(),row)==now.end()){extra+=row;++rows;}
 if(extra.empty())return true;merged.insert(at+size,extra);wr(merged,at+4,uint32_t(size+extra.size()));wr(merged,at+8+0x0c,uint32_t(rows+1));return true;
}
// objdb.rdb: the game's records plus every mod's; `notes` collects refusals for the log.
inline bool mergeDb(std::string& bytes,const std::vector<Mod>& list,std::vector<std::string>& notes){
 std::vector<Slot> slots;size_t dataAt=0;if(!table(bytes,slots,dataAt))return false;
 const std::string base=bytes;std::string body=bytes.substr(dataAt);
 std::map<uint32_t,size_t> byId;for(size_t i=0;i<slots.size();++i)if(live(slots[i],base,dataAt)&&!byId.count(slots[i].id))byId[slots[i].id]=i;
 std::map<size_t,std::string> changed;std::map<uint32_t,std::pair<uint32_t,std::string>> added;   // slot -> record; id -> (flags, record)
 bool touched=false;
 for(const auto& m:list){
  std::vector<Slot> mine;size_t mineAt=0;if(m.db.empty())continue;if(!table(m.db,mine,mineAt)){notes.push_back(m.id+": its objdb is not an .rdb");continue;}
  for(const auto& s:mine){if(!live(s,m.db,mineAt))continue;const auto record=recordOf(s,m.db,mineAt);
   if(s.id>=m.firstId&&s.id<=m.lastId){
    if(added.count(s.id)){notes.push_back(m.id+": record "+std::to_string(s.id)+" already added by another mod");continue;}
    if(byId.count(s.id)){
     // Already in the file that was read (the whole database this mod shipped before): the same item, kept as is.
     const auto there=recordOf(slots[byId[s.id]],base,dataAt);
     if(there.size()<56||record.size()<56||there.compare(40,16,record,40,16)!=0)notes.push_back(m.id+": record "+std::to_string(s.id)+" is taken by a different item");
     continue;}
    added[s.id]={s.flags,record};touched=true;continue;}
   const auto found=byId.find(s.id);if(found==byId.end()){notes.push_back(m.id+": record "+std::to_string(s.id)+" is outside its ids and not the game's");continue;}
   const auto original=recordOf(slots[found->second],base,dataAt);auto& now=changed.emplace(found->second,original).first->second;
   size_t a=0,b=0;
   if(dropChunk(original,a,b)&&dropChunk(record,a,b)){if(appendDrops(now,original,record))touched=true;}
   else if(record!=original){if(now!=original)notes.push_back(m.id+": record "+std::to_string(s.id)+" was also changed by another mod; this one wins");now=record;touched=true;}
  }
 }
 if(!touched)return false;
 // Rows: the game's order, changed records appended to the data; new ones go right after the last live slot with a
 // smaller id (the game binary-searches by id, and objdb ends with a stray slot that must stay last).
 std::vector<std::pair<Slot,bool>> rows;for(size_t i=0;i<slots.size();++i){Slot s=slots[i];const auto c=changed.find(i);
  if(c!=changed.end()&&c->second!=recordOf(s,base,dataAt)){s.offset=uint32_t(body.size());s.size=uint32_t(c->second.size());body+=c->second;}
  rows.push_back({s,live(slots[i],base,dataAt)});}
 for(const auto& [id,entry]:added){
  size_t at=0;for(size_t i=0;i<rows.size();++i)if(rows[i].second&&rows[i].first.id<id)at=i+1;
  Slot s{id,entry.first,uint32_t(body.size()),uint32_t(entry.second.size())};body+=entry.second;rows.insert(rows.begin()+std::ptrdiff_t(at),{s,true});}
 std::string out(8,'\0');wr(out,0,rd(base,0));wr(out,4,uint32_t(rows.size()*16));
 for(const auto& r:rows){char raw[16];std::memcpy(raw,&r.first,16);out.append(raw,16);}
 bytes=out+body;return true;
}
// objstrings.rdb: the game's strings, padding to each mod's first index, then its strings at its indices.
inline bool mergeStrings(std::string& bytes,const std::vector<Mod>& list,std::vector<std::string>& notes){
 if(bytes.size()<8||std::memcmp(bytes.data(),"\x02\x0c\xbf\xaf",4))return false;const uint32_t length=rd(bytes,4);if(length%16||8ull+length>bytes.size())return false;
 struct Entry {uint32_t a,b;std::string text;};std::vector<Entry> entries;const size_t pool=8+length;
 for(uint32_t i=0;i<length/16;++i){const uint32_t a=rd(bytes,8+16*i),b=rd(bytes,12+16*i),o=rd(bytes,16+16*i),s=rd(bytes,20+16*i);
  if(pool+uint64_t(o)+s>bytes.size())return false;entries.push_back({a,b,bytes.substr(pool+o,s)});}
 std::vector<const Mod*> order;for(const auto& m:list)if(!m.strings.empty())order.push_back(&m);
 std::sort(order.begin(),order.end(),[](const Mod* x,const Mod* y){return x->firstString<y->firstString;});bool placed=false;
 for(const auto* m:order){
  const auto& s=m->strings;if(s.size()<8||std::memcmp(s.data(),"\x02\x0c\xbf\xaf",4)){notes.push_back(m->id+": its objstrings is not a strings package");continue;}
  const uint32_t n=rd(s,4)/16;const size_t mp=8+rd(s,4);
  if(entries.size()>=m->firstString){notes.push_back(m->id+": its strings start at "+std::to_string(m->firstString)+" but that index is taken");continue;}
  if(m->firstString+n-1>m->lastString){notes.push_back(m->id+": it has more strings than its range");continue;}
  // A whole objstrings that a mod (or an older version of it) still installs already holds these texts: adding
  // them again would only grow the table, so a mod whose every string is present is skipped.
  bool present=n>0;
  for(uint32_t i=0;i<n&&present;++i){const uint32_t o=rd(s,16+16*i),z=rd(s,20+16*i);if(mp+uint64_t(o)+z>s.size()){present=false;break;}
   const auto text=s.substr(mp+o,z);present=std::any_of(entries.begin(),entries.end(),[&](const Entry& e){return e.text==text;});}
  if(present){notes.push_back(m->id+": its strings are already in objstrings; not added again");continue;}
  while(entries.size()+1<m->firstString)entries.push_back({0,1,std::string()});
  for(uint32_t i=0;i<n;++i){const uint32_t o=rd(s,16+16*i),z=rd(s,20+16*i);if(mp+uint64_t(o)+z>s.size())break;entries.push_back({rd(s,8+16*i),rd(s,12+16*i),s.substr(mp+o,z)});}
  placed=true;
 }
 if(!placed)return false;
 std::string head(8,'\0');std::memcpy(&head[0],"\x02\x0c\xbf\xaf",4);wr(head,4,uint32_t(entries.size()*16));std::string text;
 for(const auto& e:entries){char raw[16];const uint32_t v[4]={e.a,e.b,uint32_t(text.size()),uint32_t(e.text.size())};std::memcpy(raw,v,16);head.append(raw,16);text+=e.text;}
 bytes=head+text;return true;
}
// ---- registration (EMTK submit time) and the member-read edit ----
inline bool parseRange(const std::string& v,uint32_t& first,uint32_t& last){
 const auto a=v.find('['),c=v.find(','),z=v.find(']');if(a==std::string::npos||c==std::string::npos||z==std::string::npos||!(a<c&&c<z))return false;
 try{first=uint32_t(std::stoul(v.substr(a+1,c-a-1),nullptr,0));last=uint32_t(std::stoul(v.substr(c+1,z-c-1),nullptr,0));}catch(...){return false;}return first&&first<=last;
}
static std::vector<std::string> pending;   // log lines for the pulse
static bool registerMod(const std::string& id,const std::filesystem::path& folder,const std::string& toml,std::string& why){
 std::map<std::string,std::string> rows;bool inside=false;size_t at=0;
 while(at<toml.size()){size_t end=toml.find('\n',at);if(end==std::string::npos)end=toml.size();std::string line=toml.substr(at,end-at);at=end+1;
  while(!line.empty()&&(line.back()=='\r'||line.back()==' '))line.pop_back();const auto f=line.find_first_not_of(" \t");if(f==std::string::npos||line[f]=='#')continue;line=line.substr(f);
  if(line[0]=='['&&line.find(']')!=std::string::npos&&line.find('=')==std::string::npos){inside=line=="[script.records]";continue;}
  if(!inside)continue;const auto eq=line.find('=');if(eq==std::string::npos)continue;
  std::string k=line.substr(0,eq),v=line.substr(eq+1);while(!k.empty()&&k.back()==' ')k.pop_back();const auto vs=v.find_first_not_of(' ');v=vs==std::string::npos?"":v.substr(vs);
  const auto hash=v.find(" #");if(hash!=std::string::npos)v=v.substr(0,hash);rows[k]=v;}
 if(rows.empty())return false;
 Mod m;m.id=id;
 if(!parseRange(rows["ids"],m.firstId,m.lastId)||m.lastId>=0x10000||m.lastId-m.firstId>255){why="ids must be [first, last] within 1..0xFFFF, 256 at most";return false;}
 if(rows.count("strings")&&!parseRange(rows["strings"],m.firstString,m.lastString)){why="strings must be [first, last]";return false;}
 if(m.firstString&&(m.lastString>=65536||m.lastString-m.firstString>255)){why="strings must stay below 65536, 256 at most";return false;}
 auto readFile=[&](const char* key,std::string& out)->bool{
  if(!rows.count(key))return true;std::string v=rows[key];if(v.size()<2||v.front()!='"'||v.back()!='"')return false;v=v.substr(1,v.size()-2);
  std::error_code e;const auto p=std::filesystem::weakly_canonical(folder/std::filesystem::u8path(v),e);
  if(e||p.native().compare(0,folder.native().size()+1,(folder/L"").native())!=0)return false;
  std::ifstream in(p,std::ios::binary);if(!in)return false;out.assign(std::istreambuf_iterator<char>(in),{});return out.size()>=8&&out.size()<=4u*1024*1024;};
 if(!readFile("objdb",m.db)||!readFile("objstrings",m.strings)){why="objdb/objstrings must name files inside the addon folder (8 bytes .. 4 MiB)";return false;}
 if(!m.strings.empty()&&!m.firstString){why="objstrings needs a strings range";return false;}
 AcquireSRWLockExclusive(&lock);
 for(const auto& o:mods)if(o.id!=id&&((m.firstId<=o.lastId&&o.firstId<=m.lastId)||(m.firstString&&o.firstString&&m.firstString<=o.lastString&&o.firstString<=m.lastString)))why="its ranges overlap "+o.id+"'s";
 if(why.empty()){mods.erase(std::remove_if(mods.begin(),mods.end(),[&](const Mod& o){return o.id==id;}),mods.end());mods.push_back(std::move(m));any=true;}
 ReleaseSRWLockExclusive(&lock);return why.empty();
}
static std::string lower(std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;}
static bool wants(const char* name16){if(!any.load())return false;const auto n=lower(name16);return n=="objdb.rdb"||n=="objstrings.rdb";}
static bool edit(const char* name16,std::string& bytes){
 if(!any.load())return false;std::vector<Mod> list;AcquireSRWLockShared(&lock);list=mods;ReleaseSRWLockShared(&lock);
 std::vector<std::string> notes;const auto n=lower(name16);bool changed=false;
 const size_t before=bytes.size();
 if(n=="objdb.rdb")changed=mergeDb(bytes,list,notes);else if(n=="objstrings.rdb")changed=mergeStrings(bytes,list,notes);
 // One line per read, so a log shows whether the merge ran at all (it must run before the game reads the file).
 notes.push_back(std::string(changed?"merged into ":"read, nothing to add: ")+n+" ("+std::to_string(before)+" -> "+std::to_string(bytes.size())+" bytes)");
 if(!notes.empty()){AcquireSRWLockExclusive(&lock);for(auto& x:notes)if(pending.size()<64)pending.push_back(std::move(x));ReleaseSRWLockExclusive(&lock);}
 return changed;
}
static void deliver(scripting::Host& owner){
 std::vector<std::string> lines;AcquireSRWLockExclusive(&lock);lines.swap(pending);ReleaseSRWLockExclusive(&lock);
 for(const auto& l:lines)owner.report("runtime",l.rfind("merged",0)==0||l.rfind("read,",0)==0||l.find("are already in objstrings")!=std::string::npos?"info":"warn",("[script.records] "+l).c_str());
}
}
