#pragma once
// Live NPC state for the debug overlay: one read-only snapshot per character, read through a memory
// accessor so the same code runs against the game and against a fake heap in the offline tests.
// Offsets: static reading of the game (every read
// is checked and a failed one leaves the field unknown rather than guessed).
#include <cmath>
#include <cstdint>
#include <cstring>

namespace npc {

namespace off {
// Globals (RVAs).
constexpr uintptr_t controlled = 0x4F0830;      // the controlled character
constexpr uintptr_t world = 0x4F0BD0;           // world; +0x1C28 its character list
constexpr uintptr_t clock = 0x49B970;           // game ticks
constexpr uintptr_t motileAiVmt = 0x309960;     // TMotileAI
constexpr uintptr_t rolesDb = 0x569EA0;         // charroles database
// Character list.
constexpr uintptr_t worldList = 0x1C28, listCount = 0x10C, listArray = 0x110;
constexpr int listMax = 4096;
// Character (TMotileDynamics).
constexpr uintptr_t root = 0xAB4;               // body centre: x, height, z (scene-local, cm)
constexpr uintptr_t headPart = 0x11E0, headPose = 0x2A0;   // the head part; its pose is 12 floats: basis rows, then position
constexpr uintptr_t motion = 0x9B8, progress = 0xA28, ai = 0xD40, balance = 0xD58, health = 0xD5C, focus = 0xD74, focusCeiling = 0xD78;
constexpr uintptr_t action = 0xDF4, opponent = 0xE38, attackLock = 0xEA4, stats = 0x10A0, stamina = 0x10B8, staminaMax = 0x10BC;
constexpr uintptr_t vigour = 0x103C, bodyFlags = 0x12A4;   // the maximum for health and balance; flag 8: no health pool
// Actor stats block.
constexpr uintptr_t name = 0x0C, recordId = 0x124, roleCount = 0x128, roleFirst = 0x130;
constexpr size_t nameLength = 16;
constexpr uintptr_t roleVarCount = 0x0A, roleVars = 0x0C;   // a role entry: u32 role id, ..., u8 n at +0x0A, n u32 variables from +0x0C
// The database's slot table, sorted by id; each slot is u32 id, flags, offset, size. The slot word is
// read as a count; the only slot that could be missed is the last, which is never a role.
constexpr uintptr_t dbSlotCount = 0x08, dbSlots = 0x10, dbData = 0x18, slotSize = 16, slotId = 0, slotOffset = 2;
constexpr uintptr_t recordName = 0x08;   // a record: u32 size, u32, then its name (checked against the shipped charroles.rdb)
// TMotileAI.
constexpr uintptr_t character = 0x28, attackKind = 0x9C, oppositeSide = 0x1BC, skill = 0x1D8, memoryCount = 0x288, memory = 0x290;
constexpr uintptr_t attribute2 = 0x1C4, attribute3 = 0x1C8;   // actor attributes 2 and 3 x 0.01, the 3rd sign-flipped
constexpr uintptr_t courage = 0x7B4, lookTarget = 0x770, fleeFrom = 0x778, wantsAttack = 0x87B, casting = 0x87D;
constexpr uintptr_t mode = 0x6C, handler = 0x8E8, stress = 0x974, shock = 0x978, alarm = 0x97C, level = 0x99C, moveMode = 0x874, awake = 0x50;
// A memory entry (0x68 bytes): how this NPC sees one other character.
constexpr uintptr_t entrySize = 0x68, entryOther = 0x08, entryFlags = 0x00, entryInterest = 0x14, entryConcern = 0x1C, entryHostility = 0x24;
constexpr uintptr_t entryTrust = 0x28, entryHesitation = 0x2C, entryDanger = 0x30, entryCloseness = 0x3C, entryLevel = 0x58;
constexpr int memoryMax = 12;
// A behaviour object names its character when its kind is 1.
constexpr uintptr_t behaviourKind = 0x08, behaviourCharacter = 0x10;
// Handler entry points (RVAs).
constexpr uintptr_t handlerIdle = 0xBDB20, handlerEngage = 0xB8440, handlerFlee = 0xBE2B0, handlerFetch = 0xBCF20, handlerDormant = 0xBDBD0;
// Resting handlers the brain setup picks by mode; lie-down is also forced by a knock-down, so it does not imply mode 5.
constexpr uintptr_t handlerWander = 0xBD540, handlerFollow = 0xB5A10, handlerLieDown = 0xB15E0, handlerShortWander = 0xBD7D0;
// Set on load (0x1808A0): the player character's own brain, an explore routine rather than an NPC behaviour.
constexpr uintptr_t handlerPlayerBrain = 0xBD190;
}

enum class Handler : uint8_t { unknown, idle, engage, flee, fetch, dormant, wander, follow, lieDown, shortWander, playerBrain, other };
enum class Level : uint8_t { none, wary, provoked, fight, flee };   // the brain's engagement level 0..4
enum class Attack : uint8_t { swing, overhead, thrust, other };
enum class Mode : uint8_t { unknown, wander, stand, dormant, follow, lieDown, shortWander };

constexpr unsigned actionAttacking = 2, actionDownBit = 0x200, actionDeadBits = 0xA00;
constexpr unsigned noticedFlag = 1, seenFlag = 4;
constexpr int moveReturning = 5;
constexpr unsigned noHealthPool = 8;
constexpr float hostilityLimit = 20, concernLimit = 15;
constexpr float aboveHead = 28;   // the label sits this far up the head's own up axis
constexpr float aboveCentre = 100;   // PROVISIONAL(2026-10-16, body centre to just over a human head; inferred, not measured, ratify-by: 2026-10-30)
constexpr int rolesShown = 3, rolesMax = 64, roleSlotsMax = 4096;

// Fields stay at their "unknown" defaults when a read fails; has* flags say which groups were read.
struct Live {
 uintptr_t character = 0; uint32_t record = 0;   // record: the character's id in the level (stats+0x124)
 char name[off::nameLength + 1] = {};
 char roles[rolesShown][off::nameLength + 1] = {}; int roleCount = 0;
 float pos[3] = {}, anchor[3] = {};   // scene-local: the body centre, and where the label points
 bool isPlayer = false, hasBody = false, hasBrain = false, asleep = false;
 // body
 unsigned action = 0; int motion = -1; float progress = 0, vigour = NAN, health = NAN, balance = NAN, stamina = NAN, staminaMax = NAN, focus = NAN, focusMax = NAN;
 bool attackLocked = false, noHealth = false;
 // brain
 Handler handler = Handler::unknown; Level level = Level::none; Attack attack = Attack::swing; Mode mode = Mode::unknown;
 bool oppositeSide = false, wantsAttack = false, casting = false, returning = false;
 float skill = NAN, attribute2 = NAN, attribute3 = NAN, stress = NAN, shock = NAN, courage = NAN, alarm = NAN;
 bool fightsPlayer = false, watchesPlayer = false, fleesPlayer = false, hasOpponent = false;
 // how it sees the player (its memory entry); known = false when the player is not remembered
 struct Toward { bool known = false; unsigned flags = 0; float interest = 0, concern = 0, hostility = 0, trust = 0, hesitation = 0, danger = 0, closeness = 0; int level = 0; } you;

 bool dead() const { return (action & actionDeadBits) == actionDeadBits; }
 bool down() const { return (action & actionDownBit) != 0; }
 float fear() const { return std::isfinite(stress) && std::isfinite(shock) && std::isfinite(courage) ? (std::fmax)(0.f, stress + shock - courage) : NAN; }
};

// Mem: template <class T> bool get(uintptr_t at, T& out) const -> false when the address is not readable.
template <class Mem> bool readPointer(const Mem& m, uintptr_t at, uintptr_t& out) { return m.get(at, out) && out != 0; }

template <class Mem> uintptr_t behaviourCharacter(const Mem& m, uintptr_t behaviour) {
 uint32_t kind = 0; uintptr_t c = 0;
 return behaviour && m.get(behaviour + off::behaviourKind, kind) && kind == 1 && m.get(behaviour + off::behaviourCharacter, c) ? c : 0;
}

inline Handler handlerOf(uintptr_t rva) {
 switch (rva) {
  case off::handlerIdle: return Handler::idle;
  case off::handlerEngage: return Handler::engage;
  case off::handlerFlee: return Handler::flee;
  case off::handlerFetch: return Handler::fetch;
  case off::handlerDormant: return Handler::dormant;
  case off::handlerWander: return Handler::wander;
  case off::handlerFollow: return Handler::follow;
  case off::handlerLieDown: return Handler::lieDown;
  case off::handlerShortWander: return Handler::shortWander;
  case off::handlerPlayerBrain: return Handler::playerBrain;
  default: return rva ? Handler::other : Handler::unknown;
 }
}

inline Mode modeOf(int32_t m) {
 switch (m) { case 1: return Mode::wander; case 2: return Mode::stand; case 3: return Mode::dormant; case 4: return Mode::follow; case 5: return Mode::lieDown; case 7: return Mode::shortWander; default: return Mode::unknown; }
}

inline Attack attackOf(uint32_t kind) { return kind == 0 ? Attack::swing : kind == 8 ? Attack::overhead : kind == 0x10 ? Attack::thrust : Attack::other; }

// A name worth showing: at least one character, every one printable, nothing after the first NUL but
// padding. Anything else means the layout under it is not what was decoded.
inline bool printableName(const char (&s)[off::nameLength + 1]) {
 const size_t n = strnlen(s, off::nameLength);
 if (n == 0) return false;
 for (size_t i = 0; i < n; ++i) if (static_cast<unsigned char>(s[i]) < 0x20 || static_cast<unsigned char>(s[i]) > 0x7E) return false;
 return true;
}

// A charroles record's name, found through the game's own slot table (sorted by id).
template <class Mem> bool roleName(const Mem& m, uintptr_t base, uint32_t id, char (&out)[off::nameLength + 1]) {
 uintptr_t db = 0, slots = 0, data = 0; int32_t count = 0;
 if (!readPointer(m, base + off::rolesDb, db) || !m.get(db + off::dbSlotCount, count) || count <= 0 || count > roleSlotsMax || !readPointer(m, db + off::dbSlots, slots) ||
     !readPointer(m, db + off::dbData, data)) return false;
 int lo = 0, hi = count - 1;
 while (lo <= hi) {
  const int mid = (lo + hi) / 2; uint32_t slot[4];
  if (!m.get(slots + uintptr_t(mid) * off::slotSize, slot)) return false;
  if (slot[off::slotId] == id) {
   char raw[off::nameLength]; if (!m.get(data + slot[off::slotOffset] + off::recordName, raw)) return false;
   memcpy(out, raw, sizeof raw); out[off::nameLength] = 0;
   return printableName(out);
  }
  if (slot[off::slotId] < id) lo = mid + 1; else hi = mid - 1;
 }
 return false;
}

template <class Mem> void readRoles(const Mem& m, uintptr_t base, uintptr_t stats, Live& o) {
 int32_t count = 0; uintptr_t e = 0;
 if (!m.get(stats + off::roleCount, count) || count <= 0 || count > rolesMax || !readPointer(m, stats + off::roleFirst, e)) return;
 for (int32_t i = 0; i < count && o.roleCount < rolesShown; ++i) {
  uint32_t id = 0; uint8_t vars = 0;
  if (!m.get(e, id) || !m.get(e + off::roleVarCount, vars)) return;
  if (roleName(m, base, id, o.roles[o.roleCount])) ++o.roleCount;
  e += off::roleVars + 4 * uintptr_t(vars);
 }
}

// Above the head along its up axis; above the body centre when the body has no head part.
template <class Mem> void readAnchor(const Mem& m, uintptr_t c, Live& o) {
 uintptr_t head = 0; float pose[12] = {};
 bool ok = readPointer(m, c + off::headPart, head) && m.get(head + off::headPose, pose);
 for (float v : pose) ok = ok && std::isfinite(v);
 if (ok) for (int k = 0; k < 3; ++k) o.anchor[k] = pose[9 + k] + pose[3 + k] * aboveHead;
 else { memcpy(o.anchor, o.pos, sizeof o.pos); o.anchor[1] += aboveCentre; }
}

template <class Mem> bool readBody(const Mem& m, uintptr_t base, uintptr_t c, uint32_t now, Live& o, bool includeLabels = true) {
 float p[3];
 if (!m.get(c + off::root, p) || !std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) return false;
 memcpy(o.pos, p, sizeof p);
 uintptr_t stats = 0;
 if (readPointer(m, c + off::stats, stats)) {
  if (m.get(stats + off::name, *reinterpret_cast<char(*)[off::nameLength]>(o.name))) o.name[off::nameLength] = 0;
  m.get(stats + off::recordId, o.record);
  if (includeLabels) readRoles(m, base, stats, o);
 }
 if (includeLabels) readAnchor(m, c, o);
 uint32_t lock = 0, flags = 0;
 m.get(c + off::action, o.action); m.get(c + off::motion, o.motion); m.get(c + off::progress, o.progress);
 m.get(c + off::vigour, o.vigour); m.get(c + off::health, o.health); m.get(c + off::balance, o.balance);
 o.noHealth = m.get(c + off::bodyFlags, flags) && (flags & noHealthPool); m.get(c + off::focus, o.focus); m.get(c + off::focusCeiling, o.focusMax);
 m.get(c + off::stamina, o.stamina); m.get(c + off::staminaMax, o.staminaMax);
 o.attackLocked = m.get(c + off::attackLock, lock) && int32_t(lock - now) > 0;
 uintptr_t opponent = 0;
 o.hasOpponent = readPointer(m, c + off::opponent, opponent);
 o.hasBody = true;
 return true;
}

// The brain's memory entry for the player, 0 when it does not remember them.
template <class Mem> uintptr_t playerEntry(const Mem& m, uintptr_t ai, uintptr_t player) {
 int32_t count = 0;
 if (!player || !m.get(ai + off::memoryCount, count) || count <= 0 || count > off::memoryMax) return 0;
 for (int32_t i = 0; i < count; ++i) {
  const uintptr_t e = ai + off::memory + off::entrySize * uintptr_t(i); uintptr_t other = 0;
  if (m.get(e + off::entryOther, other) && behaviourCharacter(m, other) == player) return e;
 }
 return 0;
}

// The NPC's TMotileAI, 0 for anything else (the player, a corpse, a broken link).
template <class Mem> uintptr_t motileAi(const Mem& m, uintptr_t base, uintptr_t c) {
 uintptr_t ai = 0, vmt = 0, back = 0;
 return readPointer(m, c + off::ai, ai) && m.get(ai, vmt) && vmt == base + off::motileAiVmt && m.get(ai + off::character, back) && back == c ? ai : 0;
}

template <class Mem> void readBrain(const Mem& m, uintptr_t base, uintptr_t c, uintptr_t player, Live& o) {
 const uintptr_t ai = motileAi(m, base, c); if (!ai) return;
 uintptr_t handler = 0; int32_t level = 0, awake = 1, move = 0; uint32_t kind = 0; uint8_t flag = 0;
 int32_t mode = 0; if (m.get(ai + off::mode, mode)) o.mode = modeOf(mode);
 if (m.get(ai + off::handler, handler)) o.handler = handlerOf(handler >= base ? handler - base : 0);
 if (m.get(ai + off::level, level) && level >= 0 && level <= 4) o.level = Level(level);
 if (m.get(ai + off::attackKind, kind)) o.attack = attackOf(kind);
 int32_t side = 0; if (m.get(ai + off::oppositeSide, side)) o.oppositeSide = side == 2;
 if (m.get(ai + off::wantsAttack, flag)) o.wantsAttack = flag != 0;
 if (m.get(ai + off::casting, flag)) o.casting = flag != 0;
 if (m.get(ai + off::moveMode, move)) o.returning = move == moveReturning;
 if (m.get(ai + off::awake, awake)) o.asleep = awake == 0;
 m.get(ai + off::skill, o.skill); m.get(ai + off::attribute2, o.attribute2); m.get(ai + off::attribute3, o.attribute3); m.get(ai + off::stress, o.stress); m.get(ai + off::shock, o.shock);
 m.get(ai + off::courage, o.courage); m.get(ai + off::alarm, o.alarm);
 uintptr_t opponent = 0, look = 0, flee = 0;
 o.fightsPlayer = player && readPointer(m, c + off::opponent, opponent) && opponent == player;
 o.watchesPlayer = player && readPointer(m, ai + off::lookTarget, look) && behaviourCharacter(m, look) == player;
 o.fleesPlayer = player && readPointer(m, ai + off::fleeFrom, flee) && behaviourCharacter(m, flee) == player;
 if (const uintptr_t e = playerEntry(m, ai, player)) {
  Live::Toward& t = o.you;
  t.known = m.get(e + off::entryFlags, t.flags) && m.get(e + off::entryTrust, t.trust) && m.get(e + off::entryHesitation, t.hesitation);
  m.get(e + off::entryInterest, t.interest); m.get(e + off::entryConcern, t.concern); m.get(e + off::entryHostility, t.hostility);
  m.get(e + off::entryDanger, t.danger); m.get(e + off::entryCloseness, t.closeness); m.get(e + off::entryLevel, t.level);
 }
 o.hasBrain = true;
}

// One character: its body always, its brain when it has a TMotileAI.
template <class Mem> bool readLive(const Mem& m, uintptr_t base, uintptr_t c, uintptr_t player, Live& o) {
 o = Live(); o.character = c; o.isPlayer = c == player;
 uint32_t now = 0; m.get(base + off::clock, now);
 if (!readBody(m, base, c, now, o)) return false;
 if (!o.isPlayer) readBrain(m, base, c, player, o);
 return true;
}

// The level's characters, the player and the dead included; returns how many were written.
template <class Mem> unsigned listCharacters(const Mem& m, uintptr_t base, uintptr_t* out, unsigned capacity) {
 uintptr_t world = 0, list = 0, array = 0; int32_t count = 0;
 if (!readPointer(m, base + off::world, world) || !readPointer(m, world + off::worldList, list) || !m.get(list + off::listCount, count) ||
     count < 0 || count > off::listMax || !readPointer(m, list + off::listArray, array)) return 0;
 unsigned n = 0;
 for (int32_t i = 0; i < count && n < capacity; ++i) { uintptr_t c = 0; if (readPointer(m, array + uintptr_t(i) * 8, c)) out[n++] = c; }
 return n;
}

template <class Mem> uintptr_t controlledCharacter(const Mem& m, uintptr_t base) { uintptr_t c = 0; return readPointer(m, base + off::controlled, c) ? c : 0; }

}  // namespace npc
