#pragma once
// Hotload (r9-rc2): the checks and memory writes behind the ExaniMate panel's "Apply now" paths.
// Pure: every game address and game function is passed in, so the tests run them on owned memory.
// Facts (static reading of Exanima.exe 0.9.5.2g):
// - Package member read 0x6F2D0(package, index, buffer): entry = *(package+0x10) + index*0x20
//   (name[16], offset +0x10, size +0x14, type +0x18), count at +0x08, path string at +0x18;
//   resizes the buffer with 0x21260(buffer, size) (ptr +0, length +8, capacity +0xC, type +0x10;
//   type 0 = GetMem/ReallocMem heap), then ReadFile from the package's open handle.
// - objdb 0x104BF0 / charroles 0x26EFC0 / objstrings 0x6EEB0 copy the file verbatim:
//   u32 magic, u32 table_bytes, table (16-byte slots), body. Count at +0x08 = slots - 1 (the last
//   slot is not searched: garbage in the shipped files), table at +0x10. objdb/charroles: body
//   +0x18, size +0x20, slot = {id, flags, offset, size}; objdb ids are binary-searched (0x104660,
//   signed compare) so they must ascend. objstrings: pool +0x20, size +0x28, +0x18 reset to 0,
//   slot = {?, ?, offset, length}. charroles reads take the spin lock at +0x28 (0x17620/0x17650).
// - NPC AI (constructor 0xAFBB0, floats): +0x1D8 skill, +0x1C4, +0x1C8, +0x1D0, +0x1CC, +0x7B4.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <array>

namespace hot {

// Delivered in two chunks (coordinator 19:37 UTC): rc2a = databases, the member redirect, Reload
// level and live move/turn; rc2b turns on sounds, animations, NPC AI, looks and worn-gear refresh
// (written and unit-tested here, refused by the game side and hidden in the panel until then).
constexpr bool rc2b = true;    // rc2b: on (2026-10-15, after the rc2a-fp2 release and the dblive review)
// Live database swaps stay off (rc2a review, 2026-10-15 20:02 UTC): objstrings is not a static copy
// at run time. 0x6EC20 drops a slot's reference count (slot +4) and clears the slot at zero; 0x6ED40
// rewrites a string in place and shifts the pool; 0x104B50 (item lookups) releases an objdb record's
// name and description references into it, clearing bit 31 of record +0x1C/+0x20. Swapping either
// table resets that state under items that already hold references. Saved database edits therefore
// take effect after an install and a restart until a safe in-place path is mapped.
constexpr bool liveDatabases = false;
// What the panel asks the mod to do (frame thread, at a safe point).
enum Action { applyDb = 1, applySound, applyAi, applyLooks, applyAnims, applyLevel, applyLevelLive, applyLevelUndo, applyString, applyRecord, applyEnvironment, applyLevelRecord, applyTerrainTile, applyChestItem, applyRefreshLook, applyNpc };   // applyNpc: one NPC's AI values and feelings toward you (bytes = npc::NpcWrite)   // rc3n: name = the item's model name (worn model cache key)   // rc3d: packOp(item, {1 put in / 0 take back})
// rc3b terrain live: one tile's new heights, as the service's
// terrain_tile op names it, followed by its 244 x 244 floats (GET /api/terrain/tile.raw).
// area: x0, z0, x1, z1 (cm, the level file's frame); offset: the panel's frame check (game = level - offset)
struct TerrainTileHead { char palette[16]{}; int32_t nx = 0, ny = 0, ix = 0, iy = 0; float area[4]{}; float offset[3]{}; };
constexpr size_t terrainTileFloats = 244 * 244, terrainTileBytes = terrainTileFloats * 4;
inline bool terrainHeadOk(const TerrainTileHead& h, const char*& why) {
 if (!h.palette[0] || h.nx < 1 || h.ny < 1 || h.nx > 64 || h.ny > 64 || h.ix < 0 || h.iy < 0 || h.ix >= h.nx || h.iy >= h.ny) { why = "bad tile"; return false; }
 for (float v : h.area) if (!std::isfinite(v) || std::fabs(v) > 1e6f) { why = "bad area"; return false; }
 if (h.area[2] < h.area[0] || h.area[3] < h.area[1]) { why = "bad area"; return false; }
 for (float v : h.offset) if (!std::isfinite(v) || std::fabs(v) > 1e5f) { why = "bad offset"; return false; }
 return true;
}
// rc3d chest contents: a level-local container record is a flat
// list of chunks {u32 id, u32 size incl. the 8-byte header}; chunk 0x20000 holds a 24-byte header (count
// u32 at +20) then rows {u32 item, u32 state, i16 x, i16 y}. Adds a row at the chunk's end (state 0, the
// last row's x, y), or removes the last row holding `item`. Returns false if the layout is not that.
inline bool chestEdit(std::vector<uint8_t>& rec, uint32_t item, bool add) {
 size_t p = 0;
 while (p + 8 <= rec.size()) {
  uint32_t id, size; memcpy(&id, rec.data() + p, 4); memcpy(&size, rec.data() + p + 4, 4);
  if (size < 8 || p + size > rec.size()) return false;
  if (id == 0x20000) {
   if (size < 8 + 24) return false;
   uint32_t count; memcpy(&count, rec.data() + p + 8 + 20, 4);
   const size_t rows = p + 8 + 24; if (count > 4096 || rows + size_t(count) * 12 > p + size) return false;
   if (add) {
    uint8_t row[12]{}; memcpy(row, &item, 4); if (count) memcpy(row + 8, rec.data() + rows + size_t(count - 1) * 12 + 8, 4);
    rec.insert(rec.begin() + ptrdiff_t(rows + size_t(count) * 12), row, row + 12);
    ++count; size += 12;
   } else {
    size_t at = SIZE_MAX; for (uint32_t k = 0; k < count; ++k) { uint32_t it; memcpy(&it, rec.data() + rows + size_t(k) * 12, 4); if (it == item) at = k; }
    if (at == SIZE_MAX) return false;
    rec.erase(rec.begin() + ptrdiff_t(rows + at * 12), rec.begin() + ptrdiff_t(rows + at * 12 + 12));
    --count; size -= 12;
   }
   memcpy(rec.data() + p + 4, &size, 4); memcpy(rec.data() + p + 8 + 20, &count, 4);
   return true;
  }
  p += size;
 }
 return false;
}
// One frame's step toward the new heights: no sample moves more than `step` (stored units; the world
// is about twice that), so the physics thread never sees a big jump. Returns whether more frames are needed.
inline bool terrainStep(float* now, const float* want, size_t n, float step, float& rise) {
 bool more = false; rise = 0;
 for (size_t i = 0; i < n; ++i) {
  float d = want[i] - now[i];
  if (!std::isfinite(d)) continue;
  if (d > step) { d = step; more = true; } else if (d < -step) { d = -step; more = true; }
  if (d > rise) rise = d;
  now[i] += d;
 }
 return more;
}

// ---------------- databases ----------------
enum class Db { objdb, charroles, objstrings };
constexpr uint32_t objdbMagic = 0xDBAC0D09u, objstringsMagic = 0xAFBF0C02u;
constexpr size_t dbLimit = 64u << 20;
struct DbFile { uint32_t magic = 0, tableBytes = 0, bodyBytes = 0; int last = 0; const uint8_t* table = nullptr; const uint8_t* body = nullptr; };
struct DbFields { size_t count, table, body, size; bool resetCache; };
inline DbFields fields(Db kind) {
 return kind == Db::objstrings ? DbFields{0x08, 0x10, 0x20, 0x28, true} : DbFields{0x08, 0x10, 0x18, 0x20, false};
}
inline bool dbByName(const std::string& member, Db& kind) {
 std::string m = member; for (char& c : m) c = char(tolower((unsigned char)c));
 if (m == "objdb.rdb") { kind = Db::objdb; return true; }
 if (m == "charroles.rdb") { kind = Db::charroles; return true; }
 if (m == "objstrings.rdb") { kind = Db::objstrings; return true; }
 return false;
}
// The whole file must be one the game's loader would take as it is, and every slot the game can
// reach must point inside the new body. why: the refusal shown to the player.
inline bool checkDb(Db kind, const uint8_t* data, size_t n, uint32_t expectedMagic, DbFile& out, const char*& why) {
 out = DbFile();
 if (!data || n < 8 + 32) { why = "the file is too short"; return false; }
 if (n > dbLimit) { why = "the file is larger than 64 MB"; return false; }
 uint32_t magic, tableBytes; memcpy(&magic, data, 4); memcpy(&tableBytes, data + 4, 4);
 if (magic != expectedMagic) { why = "the file's format number is not the one the game has loaded"; return false; }
 if (tableBytes % 16 || tableBytes < 32 || uint64_t(tableBytes) + 8 >= n) { why = "the slot table size is wrong"; return false; }
 out.magic = magic; out.tableBytes = tableBytes; out.bodyBytes = uint32_t(n - 8 - tableBytes);
 out.table = data + 8; out.body = data + 8 + tableBytes; out.last = int(tableBytes / 16) - 1;
 int32_t previous = 0;
 for (int i = 0; i < out.last; ++i) {
  uint32_t slot[4]; memcpy(slot, out.table + size_t(i) * 16, 16);
  if (kind == Db::objstrings) {
   if (uint64_t(slot[2]) + slot[3] > out.bodyBytes) { why = "a string points outside the text pool"; return false; }
   continue;
  }
  const int32_t id = int32_t(slot[0]);
  if (i > 0 && id <= previous) { why = "the item ids are not in ascending order"; return false; }
  previous = id;
  if (slot[3] && uint64_t(slot[2]) + slot[3] > out.bodyBytes) { why = "a record points outside the file"; return false; }
 }
 return true;
}
// Swaps the database at obj to new copies made with the game's allocator (alloc(&p, size), the
// game's GetMem 0xED70). The old table and body are never freed: live items, roles and dialogue
// keep pointers into them. Count goes first when it shrinks and last when it grows, so a reader
// between two writes never indexes past the table it sees. Called at a safe point on the game
// thread (charroles: also under its lock, by the caller).
using Alloc = void (*)(void*, int64_t);
// rc2e (independent review #9): the new buffers are allocated and filled first (prepareDb, no lock
// held); only the four field writes (commitDb) run under a game lock.
struct DbSwap { void* table = nullptr; void* body = nullptr; };
inline bool prepareDb(const DbFile& f, Alloc alloc, DbSwap& s, const char*& why) {
 if (!alloc || !f.table || f.last < 1) { why = "nothing to swap"; return false; }
 alloc(&s.table, int64_t(f.tableBytes)); alloc(&s.body, int64_t(f.bodyBytes));
 if (!s.table || !s.body) { why = "the game could not allocate memory"; return false; }   // a new block not yet used by anything: left as it is
 memcpy(s.table, f.table, f.tableBytes); memcpy(s.body, f.body, f.bodyBytes);
 return true;
}
inline void commitDb(Db kind, uintptr_t obj, const DbFile& f, const DbSwap& s) {
 void* table = s.table; void* body = s.body;
 const DbFields o = fields(kind);
 int* count = (int*)(obj + o.count);
 const bool shrinks = f.last < *count;
 if (shrinks) *count = f.last;
 *(void**)(obj + o.table) = table;
 *(void**)(obj + o.body) = body;
 *(int*)(obj + o.size) = int(f.bodyBytes);
 if (o.resetCache) *(int*)(obj + 0x18) = 0;   // as the loader does
 if (!shrinks) *count = f.last;
}
inline bool swapDb(Db kind, uintptr_t obj, const DbFile& f, Alloc alloc, const char*& why) {
 if (!obj) { why = "nothing to swap"; return false; }
 DbSwap s; if (!prepareDb(f, alloc, s, why)) return false;
 commitDb(kind, obj, f, s); return true;
}

// ---------------- member redirect ----------------
// The active project's changed members, served by the member-read hook in place of the archive's
// bytes. A table is never changed or freed once published (the hook may be reading it on the
// texture streaming thread); a new Apply publishes a new one.
struct Member { std::string archive, name; std::vector<uint8_t> bytes; };   // archive and name lowercase
struct Table { std::string project; std::vector<Member> members; size_t bytes = 0; };
constexpr size_t memberLimit = 128u << 20, tableLimit = 512u << 20;
inline std::string lower(std::string s) { for (char& c : s) c = char(tolower((unsigned char)c)); return s; }
// A member the hook can serve: a top-level member (no '/': nested packages are rebuilt into their
// outer member first), 1..16 characters, of an .rpk archive; not empty, not over the limit.
inline bool addMember(Table& t, const std::string& archive, const std::string& name, std::vector<uint8_t>&& bytes, const char*& why) {
 const std::string a = lower(archive), m = lower(name);
 if (a.size() < 5 || a.compare(a.size() - 4, 4, ".rpk") || a.find_first_of("/\\") != std::string::npos) { why = "not an .rpk archive"; return false; }
 if (m.empty() || m.size() > 16 || m.find_first_of("/\\") != std::string::npos) { why = "not a top-level member name"; return false; }
 if (bytes.empty() || bytes.size() > memberLimit) { why = "empty or larger than 128 MB"; return false; }
 if (t.bytes + bytes.size() > tableLimit) { why = "the project's changes are larger than 512 MB in all"; return false; }
 for (const Member& e : t.members) if (e.archive == a && e.name == m) { why = "listed twice"; return false; }
 t.bytes += bytes.size(); t.members.push_back(Member{a, m, std::move(bytes)}); return true;
}
// Same members with the same bytes: an Apply that changed nothing reuses the published table
// (published tables are never freed, so each new one is kept for the session).
inline bool sameTable(const Table* a, const Table* b) {
 if (!a || !b || a->project != b->project || a->members.size() != b->members.size()) return false;
 for (size_t i = 0; i < a->members.size(); ++i) { const Member& x = a->members[i]; const Member& y = b->members[i]; if (x.archive != y.archive || x.name != y.name || x.bytes != y.bytes) return false; }
 return true;
}
// Package path (as the game opened it) -> lowercase file name.
inline const char* fileName(const char* path) {
 const char* n = path; for (const char* p = path; *p; ++p) if (*p == '\\' || *p == '/' || *p == ':') n = p + 1; return n;
}
inline const Member* findMember(const Table* t, const char* packagePath, const unsigned char* entryName16) {
 if (!t || t->members.empty() || !packagePath || !entryName16) return nullptr;
 const char* file = fileName(packagePath);
 char entry[17]{}; memcpy(entry, entryName16, 16);
 for (const Member& m : t->members) if (!_stricmp(m.archive.c_str(), file) && !_stricmp(m.name.c_str(), entry)) return &m;
 return nullptr;
}
// Fills the game's buffer with the member's bytes the way the original read does (resize with the
// game's own 0x21260, which also sets the length), for plain heap buffers only (type 0 or 1).
using Resize = char (*)(void*, unsigned);
inline bool serve(void* buffer, const Member& m, Resize resize, const char*& why) {
 if (!buffer || !resize) { why = "no buffer"; return false; }
 const unsigned type = *(const uint32_t*)((const uint8_t*)buffer + 0x10) & 0x3f;
 if (type > 1) { why = "the game asked for a kind of buffer the redirect does not fill"; return false; }
 if (!resize(buffer, unsigned(m.bytes.size()))) { why = "the game could not size the buffer"; return false; }
 void* data = *(void**)buffer;
 if (!data || *(const uint32_t*)((const uint8_t*)buffer + 8) != m.bytes.size()) { why = "the buffer did not take the new size"; return false; }
 memcpy(data, m.bytes.data(), m.bytes.size());
 return true;
}

// ---------------- packages (nested members such as Resource.rpk/actors.rpk/opponent08) ----------------
// Rebuilds a package with some members replaced or added: same header magic, the table sorted by
// name (byte order, as every shipped package), members back to back after it, each entry's extra
// 8 bytes kept. RPK.md: u8 magic[4], u32 table_bytes, 32-byte entries {name[16], offset (from the
// table end), size, extra[8]}, data.
constexpr uint32_t packageMagic = 0xAFBF0C01u;
inline bool rebuildPackage(const std::vector<uint8_t>& base, const std::map<std::string, std::vector<uint8_t>>& changes, std::vector<uint8_t>& out, const char*& why) {
 out.clear();
 if (base.size() < 8) { why = "not a package"; return false; }
 uint32_t magic, tableBytes; memcpy(&magic, base.data(), 4); memcpy(&tableBytes, base.data() + 4, 4);
 if (magic != packageMagic || tableBytes % 32 || uint64_t(tableBytes) + 8 > base.size()) { why = "not a package"; return false; }
 struct Entry { std::string name; uint8_t extra[8]; const uint8_t* data; size_t size; };
 std::map<std::string, Entry> entries;   // keyed by the stored name: std::map orders bytes like Python's sorted()
 const size_t dataStart = 8 + size_t(tableBytes);
 for (size_t i = 0; i < tableBytes / 32; ++i) {
  const uint8_t* e = base.data() + 8 + i * 32; char name[17]{}; memcpy(name, e, 16);
  uint32_t offset, size; memcpy(&offset, e + 16, 4); memcpy(&size, e + 20, 4);
  if (!name[0] || uint64_t(dataStart) + offset + size > base.size()) { why = "a member points outside the package"; return false; }
  Entry x; x.name = name; memcpy(x.extra, e + 24, 8); x.data = base.data() + dataStart + offset; x.size = size;
  if (!entries.emplace(x.name, x).second) { why = "a member is listed twice"; return false; }
 }
 for (const auto& c : changes) {
  if (c.first.empty() || c.first.size() > 16 || c.first.find_first_of("/\\") != std::string::npos) { why = "a changed member has a bad name"; return false; }
  std::string key = c.first;   // the package's own spelling when it has the member
  for (const auto& e : entries) if (!_stricmp(e.first.c_str(), c.first.c_str())) { key = e.first; break; }
  Entry x{}; x.name = key; auto had = entries.find(key); if (had != entries.end()) memcpy(x.extra, had->second.extra, 8);
  x.data = c.second.data(); x.size = c.second.size(); entries[key] = x;
 }
 size_t total = 0; for (const auto& e : entries) total += e.second.size;
 if (total > memberLimit) { why = "the rebuilt package is larger than 128 MB"; return false; }
 const uint32_t newTable = uint32_t(entries.size() * 32);
 out.resize(8 + size_t(newTable) + total);
 memcpy(out.data(), &magic, 4); memcpy(out.data() + 4, &newTable, 4);
 size_t i = 0, at = 0;
 for (const auto& kv : entries) {
  uint8_t* e = out.data() + 8 + i * 32; memset(e, 0, 32); memcpy(e, kv.second.name.data(), kv.second.name.size());
  const uint32_t offset = uint32_t(at), size = uint32_t(kv.second.size); memcpy(e + 16, &offset, 4); memcpy(e + 20, &size, 4); memcpy(e + 24, kv.second.extra, 8);
  if (size) memcpy(out.data() + 8 + newTable + at, kv.second.data, size);
  at += size; ++i;
 }
 return true;
}
// Reads one member of a package (tests and checks).
inline bool packageMember(const std::vector<uint8_t>& p, const std::string& name, std::vector<uint8_t>& out) {
 out.clear(); if (p.size() < 8) return false; uint32_t tableBytes; memcpy(&tableBytes, p.data() + 4, 4);
 if (uint64_t(tableBytes) + 8 > p.size()) return false;
 for (size_t i = 0; i < tableBytes / 32; ++i) { const uint8_t* e = p.data() + 8 + i * 32; char n[17]{}; memcpy(n, e, 16);
  if (_stricmp(n, name.c_str())) continue; uint32_t offset, size; memcpy(&offset, e + 16, 4); memcpy(&size, e + 20, 4);
  if (uint64_t(8) + tableBytes + offset + size > p.size()) return false; out.assign(p.data() + 8 + tableBytes + offset, p.data() + 8 + tableBytes + offset + size); return true; }
 return false;
}

// ---------------- NPC AI ----------------
// Actor-file units (docs/ACTOR.md): AI skill 0..100, attributes -100..100 (2 and 3 feed the AI).
struct AiInput { double skill = 0, attribute2 = 0, attribute3 = 0; };
inline bool aiInputOk(const AiInput& in, const char*& why) {
 if (!std::isfinite(in.skill) || in.skill < 0 || in.skill > 100) { why = "AI skill must be 0 to 100"; return false; }
 for (double a : {in.attribute2, in.attribute3}) if (!std::isfinite(a) || a < -100 || a > 100) { why = "attributes must be -100 to 100"; return false; }
 return true;
}
// The constructor's constants (0xAFBB0 asm: 0x30A050 double 0.01, floats 0x30A068 1, 0x30A038 0.5,
// 0x30A070 2, 0x30A1A0 3, 0x30A328 0.125, double 0x30A490 0.333), read from the game image so the
// arithmetic is the game's own; a different value means a different build: refuse.
struct AiConstants { double hundredth; float one, half, two, three, eighth; double third; };
inline bool aiConstantsOk(const AiConstants& k) {
 return k.hundredth == 0.01 && k.one == 1.f && k.half == .5f && k.two == 2.f && k.three == 3.f && k.eighth == .125f &&
        std::isfinite(k.third) && k.third > 0 && k.third <= 1;
}
struct AiFields { float skill, a2, a3, blend, weight, mood; };   // +0x1D8, +0x1C4, +0x1C8, +0x1D0, +0x1CC, +0x7B4
inline constexpr std::array<uintptr_t,6> aiOffsets={0x1d8,0x1c4,0x1c8,0x1d0,0x1cc,0x7b4};
inline std::array<float,6> aiValues(const AiFields& f){return {f.skill,f.a2,f.a3,f.blend,f.weight,f.mood};}
inline void commitAi(uintptr_t ai,const AiFields& fields){
 const auto values=aiValues(fields);for(size_t i=0;i<values.size();++i)memcpy((void*)(ai+aiOffsets[i]),&values[i],sizeof(float));
}
// The constructor's sequence, operation for operation (float vs double as in its asm).
// statsBlend: the character's stats +0x90 (0 in every shipped actor); fixed: character +0x12A4 & 4.
inline AiFields aiCompute(const AiInput& in, const AiConstants& k, float statsBlend, bool fixed) {
 AiFields f{};
 f.skill = float(double(float(in.skill)) * k.hundredth); if (f.skill > k.one) f.skill = k.one;
 f.a2 = float(double(float(in.attribute2)) * k.hundredth);
 f.a3 = float(double(-float(in.attribute3)) * k.hundredth);
 f.mood = (k.one + f.a2) * k.half;
 if (fixed) f.blend = k.one;
 else { float g = k.one - statsBlend; g *= g; f.blend = float(double(k.one - g) + double(g) * (double(k.two + f.a2) * k.third)); }
 const float x = k.three + f.a2; float y = k.three - f.a3; y += x; f.weight = y * k.eighth;
 return f;
}

// ---------------- actor files (Resource.rpk/actors.rpk/<name>) ----------------
// u32 magic 0x00CDAC06/07, u32 header_bytes, header. The game's reader 0x114A20 copies header
// [0,16) name -> stats +0x0C, [32,40) physique -> +0x2C, [40] height -> +0x34, [44] age -> +0x38,
// [48] skin tone -> +0x3C, [52] hair -> +0x40, [56] hair colour -> +0x44; the AI skill float and
// the attribute bytes (stored +100) follow a variable-length block.
struct Actor { char name[17]{}; uint8_t looks[20]{}; bool aiKnown = false; AiInput ai; };
inline bool readActor(const uint8_t* d, size_t n, Actor& a, const char*& why) {
 a = Actor();
 if (!d || n < 8 + 96) { why = "the actor file is too short"; return false; }
 uint32_t magic, headerBytes; memcpy(&magic, d, 4); memcpy(&headerBytes, d + 4, 4);
 if (magic != 0x00CDAC06u && magic != 0x00CDAC07u) { why = "not an actor file"; return false; }
 if (headerBytes < 96 || uint64_t(headerBytes) + 8 > n) { why = "the actor header size is wrong"; return false; }
 const uint8_t* h = d + 8;
 memcpy(a.name, h, 16); if (!a.name[0]) { why = "the actor has no name"; return false; }
 for (int i = 0; i < 16 && a.name[i]; ++i) if ((unsigned char)a.name[i] < 0x20 || (unsigned char)a.name[i] > 0x7e) { why = "the actor name is not text"; return false; }
 memcpy(a.looks, h + 32, 20);
 for (int i = 0; i < 4; ++i) { float v; memcpy(&v, h + 32 + 4 * i, 4); if (!std::isfinite(v) || std::fabs(v) > 4) { why = "the body values are out of range"; return false; } }
 // name 16, 16, 8, 5 x u32, 2 x u16, code 16, 3 x u32, 16, 16, n, n x 8, 5 x u32 (AI skill 4th), 4 attribute bytes
 size_t at = 16 + 16 + 8 + 20 + 4 + 16 + 12 + 16 + 16;
 if (at + 4 > headerBytes) { a.aiKnown = false; return true; }   // a short header: no AI block (never read past it)
 uint32_t count; memcpy(&count, h + at, 4); at += 4;
 if (count > 4096 || at + uint64_t(count) * 8 + 20 + 4 > headerBytes) { a.aiKnown = false; return true; }
 at += size_t(count) * 8;
 float skill; memcpy(&skill, h + at + 12, 4); at += 20;
 a.ai.skill = skill; a.ai.attribute2 = int(h[at + 1]) - 100; a.ai.attribute3 = int(h[at + 2]) - 100;
 const char* ignored; a.aiKnown = aiInputOk(a.ai, ignored);
 return true;
}
// The live stats block came from this actor: its name, and its body values equal the shipped
// file's or the project's (so a different NPC that shares the name by chance is left alone).
inline bool statsFromActor(const uint8_t* stats, const Actor& shipped, const Actor& project) {
 if (_strnicmp((const char*)stats + 0x0C, shipped.name, 16)) return false;
 return !memcmp(stats + 0x2C, shipped.looks, 20) || !memcmp(stats + 0x2C, project.looks, 20);
}

// ---------------- sounds (Sound.rpk members are RIFF WAVE files) ----------------
// The game's decoder 0xCFB80: chunks after "WAVE"; "fmt " must be PCM (1); mono/stereo x 8/16 bit
// -> AL_FORMAT 0x1100..0x1103; rate = fmt +4; "data" -> the samples. Bounds checked here (the game
// does not).
struct Wav { int format = 0; const uint8_t* data = nullptr; uint32_t size = 0, rate = 0; };
inline bool parseWav(const uint8_t* d, size_t n, Wav& w, const char*& why) {
 w = Wav();
 if (!d || n < 12 || memcmp(d + 8, "WAVE", 4)) { why = "not a WAVE sound"; return false; }
 bool fmt = false; size_t at = 12;
 while (at + 8 <= n) {
  uint32_t size; memcpy(&size, d + at + 4, 4);
  if (uint64_t(at) + 8 + size > n) { why = "a chunk runs past the end of the sound"; return false; }
  const uint8_t* c = d + at + 8;
  if (!memcmp(d + at, "fmt ", 4)) {
   if (size < 16) { why = "the sound's format block is short"; return false; }
   uint16_t tag, channels, bits; memcpy(&tag, c, 2); memcpy(&channels, c + 2, 2); memcpy(&w.rate, c + 4, 4); memcpy(&bits, c + 14, 2);
   if (tag != 1) { why = "the sound is not plain PCM"; return false; }
   if ((channels != 1 && channels != 2) || (bits != 8 && bits != 16) || w.rate < 1000 || w.rate > 192000) { why = "the sound's channels, bits or rate are not supported"; return false; }
   w.format = channels == 1 ? (bits == 8 ? 0x1100 : 0x1101) : (bits == 8 ? 0x1102 : 0x1103); fmt = true;
  } else if (!memcmp(d + at, "data", 4) && fmt) { w.data = c; w.size = size; }
  at += 8 + size + (size & 1);
 }
 if (!fmt || !w.data || !w.size) { why = "the sound has no samples"; return false; }
 return true;
}
// The cache entry's play length (+0x08) as the loader 0xCC520 computes it (0x30BC00 1000,
// 0x30BC08 0.5, 0x30BC10 0.25).
inline int wavMilliseconds(const Wav& w) {
 double s = double(w.size) / double(w.rate) * 1000.0;
 if (w.format == 0x1101 || w.format == 0x1102) s = double(w.size) / double(w.rate) * 0.5 * 1000.0;
 if (w.format == 0x1103) s = double(w.size) / double(w.rate) * 0.25 * 1000.0;
 return int(std::llround(s));
}

// ---------------- when a live write is allowed ----------------
// A level is settled when a player character exists, the scene instance has not changed for 3 s
// and no level name is pending (0x1250A0 writes the requested name before the load runs).
inline bool settled(bool playerExists, unsigned long long sceneStableMs, const char* sceneName, const char* pendingName) {
 if (!playerExists || sceneStableMs < 3000 || !sceneName || !sceneName[0]) return false;
 return !pendingName || !pendingName[0] || !_strnicmp(sceneName, pendingName, 16);
}

// ---------------- the level you are in (LevelLive) ----------------
// A move or turn of one placed object or loose item, found in the loaded scene by name (objects,
// node +0x140) or item id (items, +0x180) and its saved position (+0x70 floats 9..11) = was.
struct LiveOp {
 int32_t op = 0;            // 1 move to pos, 2 turn by degrees, 3 set the whole transform (rot + pos; undo, service ops),
                            // 4 remove, 5 add an object (newName, rot, pos, flags, tail1), 6 replace an object (was -> newName), 7 light
 int32_t kind = 0;          // 1 object, 2 item, 3 light
 char name[16]{};           // objects
 uint32_t item = 0;         // items (a level-local one also matches with bit 31 set)
 float was[3]{}, pos[3]{}, degrees = 0;
 uint32_t seq = 0;          // the panel's number for this live change (Undo names it)
 float rot[9]{};            // op 3/5/6: the 3x3 as the level file stores it (rows; tilt and scale kept)
 char newName[16]{};        // op 5/6: the Objlib name to build
 uint32_t flags = 0, tail1 = 0;   // op 5/6: the file's node flags and tail word 1 (rc2d; tail word 0 must be 0)
 float light[9]{};          // op 7: colour r g b, strength (+0x21C), falloff (+0x2C8), range (+0x224), spread (+0x22C), aim a/b (+0x240/+0x244)
 uint32_t lightMask = 0;    // op 7: which of light[] are given (bit i); bit 10: pos is given
};
static_assert(sizeof(LiveOp) == 160, "LiveOp travels as bytes");
// A 16-byte name field as the game has it: all 16 characters kept, no terminator needed (rc2e, review Low).
inline void name16(char* out, const std::string& s) { memset(out, 0, 16); memcpy(out, s.data(), s.size() < 16 ? s.size() : 16); }
inline bool liveOpOk(const LiveOp& o, const char*& why) {
 if (o.op < 1 || o.op > 7) { why = "unknown change"; return false; }
 if ((o.op == 5 || o.op == 6) && (!o.newName[0] || o.kind != 1)) { why = "no object to build"; return false; }
 if (o.op == 7) { if (o.kind != 3) { why = "not a light"; return false; }
  for (float v : o.light) if (!std::isfinite(v) || std::fabs(v) > 1e6f) { why = "bad light value"; return false; } }
 if (o.op == 3 || o.op == 5 || o.op == 6) { for (float v : o.rot) if (!std::isfinite(v) || std::fabs(v) > 100) { why = "bad rotation"; return false; }
  for (int r = 0; r < 3; ++r) { const float n = o.rot[r * 3] * o.rot[r * 3] + o.rot[r * 3 + 1] * o.rot[r * 3 + 1] + o.rot[r * 3 + 2] * o.rot[r * 3 + 2]; if (n < 1e-4f) { why = "bad rotation"; return false; } } }
 if (o.op != 5 && o.kind != 3 && (o.kind == 1 ? !o.name[0] : o.kind == 2 ? o.item == 0 : true)) { why = "nothing named"; return false; }
 for (float v : o.was) if (!std::isfinite(v) || std::fabs(v) > 1e6f) { why = "bad position"; return false; }
 if (o.op == 1 || o.op == 3 || o.op == 5 || o.op == 6 || (o.op == 7 && (o.lightMask & 0x400))) for (float v : o.pos) if (!std::isfinite(v) || std::fabs(v) > 1e6f) { why = "bad position"; return false; }
 if (o.op == 2 && (!std::isfinite(o.degrees) || std::fabs(o.degrees) > 360)) { why = "bad turn"; return false; }
 return true;
}
// level.py Placed.turn_by: each row (x, y, z) of the 3x3 becomes (x c - z s, y, x s + z c), so tilt
// and scale stay (the service saves the same matrix).
inline void turnRows(float* rot9, float degrees) {
 const double a = double(degrees) * 3.14159265358979323846 / 180.0, c = std::cos(a), s = std::sin(a);
 for (int r = 0; r < 3; ++r) { const double x = rot9[r * 3], z = rot9[r * 3 + 2]; rot9[r * 3] = float(x * c - z * s); rot9[r * 3 + 2] = float(x * s + z * c); }
}
inline float distance3(const float* a, const float* b) { const double x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return float(std::sqrt(x * x + y * y + z * z)); }

// The takes-effect tag this build can honour for a project change (the service's tag, corrected).
// rc2b: objdb and objstrings are live through the in-place ops (/api/mod/dblive: StringsLive,
// RecordsLive), charroles by a fresh-buffer swap; the whole-file swap (liveDatabases) stays off.
inline std::string effectiveWhen(const std::string& archive, const std::string& member, const std::string& when) {
 if constexpr (!rc2b) {
  if (when == "live" && (lower(archive) == "sound.rpk" || lower(member) == "humanoid.rsq")) return "restart";
 }
 return when;
}

// ---------------- databases in place (rc2b) ----------------
// StringsLive op bytes: u32 index (1-based, as record +0x1C/+0x20 hold it), then the text as the game
// stores it (ANSI). RecordsLive op bytes: u32 objdb id, then the whole record.
inline std::vector<uint8_t> packOp(uint32_t key, const void* data, size_t n) {
 std::vector<uint8_t> v(4 + n); memcpy(v.data(), &key, 4); if (n) memcpy(v.data() + 4, data, n); return v;
}
inline bool unpackOp(const std::vector<uint8_t>& v, uint32_t& key, const uint8_t*& data, size_t& n) {
 if (v.size() < 4) return false; memcpy(&key, v.data(), 4); data = v.data() + 4; n = v.size() - 4; return true;
}
// Text for 0x6ED40: never empty (length 0 releases the slot), no NUL, at most 2000 bytes.
inline bool textOk(const uint8_t* s, size_t n, const char*& why) {
 if (!n) { why = "empty text would release the string"; return false; }
 if (n > 2000) { why = "the text is longer than 2000 characters"; return false; }
 for (size_t i = 0; i < n; ++i) if (!s[i]) { why = "the text holds a NUL"; return false; }
 return true;
}
// rc2d chest records (the level's own item list): the look chunk 0x100000 holds the name and
// description at chunk +0x1C/+0x20 as level-pool string indices with references held. The live
// record's two words are kept (the file's indices mean nothing in the live pool). False: the new
// record has a look chunk the live one lacks (or a short one): nothing safe to keep, so refuse.
inline const uint8_t* lookChunk(const uint8_t* r, size_t n) {
 size_t at = 0;
 while (at + 8 <= n) { uint32_t t, s; memcpy(&t, r + at, 4); memcpy(&s, r + at + 4, 4); if (s < 8 || at + s > n) return nullptr; if (t == 0x100000) return s >= 0x24 ? r + at : nullptr; at += s; }
 return nullptr;
}
// rc2d review: a chest record may change size (rows added or taken out), but it must keep the live
// record's chunk tags in the same order and walk cleanly to its end: the table's tag mask (slot +4,
// tested by 0x1041F0 before any chunk lookup) then stays right, and malformed bytes are refused.
inline bool sameTags(const uint8_t* live, size_t liveSize, const uint8_t* next, size_t nextSize, const char*& why) {
 size_t a = 0, b = 0; int chunks = 0;
 while (a < liveSize || b < nextSize) {
  if (a + 8 > liveSize || b + 8 > nextSize) { why = "the record's chunks changed"; return false; }
  uint32_t ta, sa, tb, sb; memcpy(&ta, live + a, 4); memcpy(&sa, live + a + 4, 4); memcpy(&tb, next + b, 4); memcpy(&sb, next + b + 4, 4);
  if (sa < 8 || a + sa > liveSize || sb < 8 || b + sb > nextSize) { why = "a chunk size is wrong"; return false; }
  if (ta != tb) { why = "the record's chunks changed"; return false; }
  a += sa; b += sb; if (++chunks > 4096) { why = "too many chunks"; return false; }
 }
 return chunks > 0;
}
inline bool keepLookStrings(const uint8_t* live, size_t liveSize, uint8_t* next, size_t nextSize) {
 uint8_t* to = const_cast<uint8_t*>(lookChunk(next, nextSize)); const uint8_t* from = lookChunk(live, liveSize);
 if (!to) return true;           // no name fields in the new record
 if (!from) return false;
 memcpy(to + 0x1C, from + 0x1C, 8); return true;
}
// An objdb record is a list of chunks {u32 tag, u32 size (with its 8-byte head)}. A copy in place is
// safe only when the new record has exactly the live one's chunks: same tags, order and sizes
// (live objects keep pointers into chunks, 0x2749C0), and both walk to the end cleanly.
inline bool sameChunks(const uint8_t* live, const uint8_t* next, size_t n, const char*& why) {
 size_t at = 0; int chunks = 0;
 while (at < n) {
  if (at + 8 > n) { why = "a chunk head runs past the record"; return false; }
  uint32_t ta, sa, tb, sb; memcpy(&ta, live + at, 4); memcpy(&sa, live + at + 4, 4); memcpy(&tb, next + at, 4); memcpy(&sb, next + at + 4, 4);
  if (ta != tb || sa != sb) { why = "the record's chunk layout changed (needs a restart)"; return false; }
  if (sa < 8 || at + sa > n) { why = "a chunk size is wrong"; return false; }
  at += sa; if (++chunks > 4096) { why = "too many chunks"; return false; }
 }
 return chunks > 0;
}


}  // namespace hot
