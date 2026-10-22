#pragma once
// Save safety for the ExaniMate editor (r9-rc2b, rc2e): one copy of the save folder (the game's own, else %APPDATA%\Exanima)
// before the editor's first change in a session, and a restore the player asks for at the main menu.
// Restores are never automatic; only the newest two backups are kept beside the mod. rc2e (independent review #10): every skipped or failed file, folder
// and move is counted (a backup with any failure is not "made"); long paths use \\?\; a backup taken
// while the game writes a save is retaken; a restore is built in a staging folder next to the saves
// and swapped in by renaming folders, the folder it replaces is kept whole beside it.
#include <windows.h>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace saves {

// The folder the running game saves to, once the host has read it (see adoptGameFolder); patched builds
// such as Hellmode keep saves beside the install instead of in %APPDATA%.
inline std::wstring& gameFolder() { static std::wstring value; return value; }
inline std::wstring folder() {
 if (!gameFolder().empty()) return gameFolder();
 std::vector<wchar_t> app(32768); const DWORD n = GetEnvironmentVariableW(L"APPDATA", app.data(), DWORD(app.size()));
 if (!n || n >= app.size()) return {};
 return std::wstring(app.data()) + L"\\Exanima";
}
// \\?\ form of an absolute drive path (no MAX_PATH limit); anything else as it is.
inline std::wstring longPath(std::wstring p) {
 for (wchar_t& c : p) if (c == L'/') c = L'\\';
 if (p.size() >= 4 && p.compare(0, 4, L"\\\\?\\") == 0) return p;
 if (p.size() >= 3 && p[1] == L':' && p[2] == L'\\') return L"\\\\?\\" + p;
 return p;
}
inline bool exists(const std::wstring& p) { return GetFileAttributesW(longPath(p).c_str()) != INVALID_FILE_ATTRIBUTES; }
// Copies the game's save-directory string: `global` (0x4985d0 in the supported build) points at a
// zero-terminated ANSI path ending in '\'.
inline bool readGamePath(uintptr_t global, char (&out)[MAX_PATH]) {
 __try {
  const char* text = *reinterpret_cast<const char* const*>(global);
  if (!text) return false;
  for (size_t i = 0; i < MAX_PATH; ++i) { out[i] = text[i]; if (!text[i]) return i > 0; }
 } __except (EXCEPTION_EXECUTE_HANDLER) {}
 return false;
}
// Uses the game's own save folder for backups when it names an existing folder; otherwise keeps %APPDATA%.
inline bool adoptGameFolder(uintptr_t global) {
 char text[MAX_PATH]{}; if (!global || !readGamePath(global, text)) return false;
 wchar_t wide[MAX_PATH]{}; const int n = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, text, -1, wide, MAX_PATH);
 if (n <= 1) return false;
 std::wstring path(wide); while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
 const DWORD attributes = GetFileAttributesW(longPath(path).c_str());
 if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
 gameFolder() = path; return true;
}
inline std::wstring stamp() { SYSTEMTIME t; GetLocalTime(&t); wchar_t b[40]; swprintf_s(b, L"%04d%02d%02d-%02d%02d%02d-%03d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds); return b; }
// Copies a tree; counts files copied and anything that could not be copied (a folder that could not
// be made or listed, a link, nesting past 16). overwrite false: an existing file is kept.
inline void copyTree(const std::wstring& from, const std::wstring& to, bool overwrite, unsigned& copied, unsigned& failed, int depth = 0) {
 if (depth > 16) { ++failed; return; }
 const std::wstring f0 = longPath(from), t0 = longPath(to);
 if (!CreateDirectoryW(t0.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) { ++failed; return; }
 WIN32_FIND_DATAW f; HANDLE h = FindFirstFileW((f0 + L"\\*").c_str(), &f); if (h == INVALID_HANDLE_VALUE) { ++failed; return; }
 do {
  const std::wstring name = f.cFileName; if (name == L"." || name == L"..") continue;
  if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ++failed; continue; }   // never followed out of the folder; counted, not hidden
  if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) copyTree(from + L"\\" + name, to + L"\\" + name, overwrite, copied, failed, depth + 1);
  else if (CopyFileW((f0 + L"\\" + name).c_str(), (t0 + L"\\" + name).c_str(), !overwrite)) ++copied; else if (overwrite || GetLastError() != ERROR_FILE_EXISTS) ++failed;
 } while (FindNextFileW(h, &f));
 FindClose(h);
}
// Every file under a folder: relative path, size and last write time (to see the game writing).
struct Entry { std::wstring path; uint64_t size = 0, written = 0; bool operator==(const Entry& o) const { return path == o.path && size == o.size && written == o.written; } };
inline bool listTree(const std::wstring& dir, std::vector<Entry>& out, const std::wstring& rel = L"", int depth = 0) {
 if (depth > 16) return false;
 WIN32_FIND_DATAW f; HANDLE h = FindFirstFileW((longPath(dir) + L"\\*").c_str(), &f); if (h == INVALID_HANDLE_VALUE) return false;
 bool ok = true;
 do {
  const std::wstring name = f.cFileName; if (name == L"." || name == L"..") continue;
  if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ok = false; continue; }
  if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { ok = listTree(dir + L"\\" + name, out, rel + name + L"\\", depth + 1) && ok; continue; }
  out.push_back({rel + name, (uint64_t(f.nFileSizeHigh) << 32) | f.nFileSizeLow, (uint64_t(f.ftLastWriteTime.dwHighDateTime) << 32) | f.ftLastWriteTime.dwLowDateTime});
 } while (FindNextFileW(h, &f));
 FindClose(h);
 return ok;
}
inline bool sameFiles(std::vector<Entry> a, std::vector<Entry> b, bool times) {
 if (a.size() != b.size()) return false;
 auto byPath = [](const Entry& x, const Entry& y) { return _wcsicmp(x.path.c_str(), y.path.c_str()) < 0; };
 std::sort(a.begin(), a.end(), byPath); std::sort(b.begin(), b.end(), byPath);   // enumeration order is the file system's (exFAT, FAT differ)
 for (size_t i = 0; i < a.size(); ++i) if (_wcsicmp(a[i].path.c_str(), b[i].path.c_str()) || a[i].size != b[i].size || (times && a[i].written != b[i].written)) return false;
 return true;
}
// Creates a folder and any missing parents.
inline void makeFolders(const std::wstring& path) {
 for (size_t i = 3; i <= path.size(); ++i) if (i == path.size() || path[i] == L'\\' || path[i] == L'/') CreateDirectoryW(longPath(path.substr(0, i)).c_str(), nullptr);
}
// Files in `live` that the backup does not have are moved to `aside` (the game made them after the
// backup). Moves that fail are counted.
inline void moveNew(const std::wstring& live, const std::wstring& backup, const std::wstring& aside, unsigned& moved, unsigned& failed, int depth = 0) {
 if (depth > 16) { ++failed; return; }
 WIN32_FIND_DATAW f; HANDLE h = FindFirstFileW((longPath(live) + L"\\*").c_str(), &f); if (h == INVALID_HANDLE_VALUE) { ++failed; return; }
 do {
  const std::wstring name = f.cFileName; if (name == L"." || name == L"..") continue;
  if (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { ++failed; continue; }
  if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { moveNew(live + L"\\" + name, backup + L"\\" + name, aside + L"\\" + name, moved, failed, depth + 1); continue; }
  if (!exists(backup + L"\\" + name)) {
   makeFolders(aside);
   if (MoveFileExW(longPath(live + L"\\" + name).c_str(), longPath(aside + L"\\" + name).c_str(), MOVEFILE_COPY_ALLOWED)) ++moved; else ++failed;
  }
 } while (FindNextFileW(h, &f));
 FindClose(h);
}
// directory: where backups go (the mod's folder, with a trailing backslash). No save folder at all:
// nothing to protect (true, made empty). The copy is retaken (up to 3 times) when the game changed a
// save while it ran; it counts as made only when nothing failed and the copy has every file.
inline bool backup(const std::wstring& directory, std::wstring& made, unsigned& copied, unsigned& failed) {
 const std::wstring from = folder(); copied = failed = 0; made.clear();
 if (from.empty() || directory.empty()) { failed = 1; return false; }
 if (!exists(from)) return true;
 for (int attempt = 0; attempt < 3; ++attempt) {
  std::vector<Entry> before, after, copy; copied = failed = 0;
  if (!listTree(from, before)) { failed = 1; return false; }
  made = directory + L"saves-backup-" + stamp();
  auto setAside = [&](const wchar_t* why) { MoveFileExW(longPath(made).c_str(), longPath(made + why).c_str(), 0); };   // never taken for a backup; not deleted
  copyTree(from, made, false, copied, failed);
  if (failed) { setAside(L"-incomplete"); made.clear(); return false; }
  if (!listTree(from, after)) { failed = 1; setAside(L"-incomplete"); made.clear(); return false; }
  if (!sameFiles(before, after, true)) { setAside(L"-superseded"); continue; }   // the game wrote a save meanwhile: take it again
  if (!listTree(made, copy) || !sameFiles(before, copy, false) || copied != before.size()) { failed = 1; setAside(L"-incomplete"); made.clear(); return false; }
  return true;
 }
 failed = 1; made.clear(); return false;
}
// Removes a folder and everything in it; a link is removed itself, never followed. False if anything stayed.
inline bool removeTree(const std::wstring& dir, int depth = 0) {
 if (depth > 16) return false;
 bool ok = true;
 WIN32_FIND_DATAW f; HANDLE h = FindFirstFileW((longPath(dir) + L"\\*").c_str(), &f);
 if (h != INVALID_HANDLE_VALUE) {
  do {
   const std::wstring name = f.cFileName, path = dir + L"\\" + name; if (name == L"." || name == L"..") continue;
   if (f.dwFileAttributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(longPath(path).c_str(), f.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
   if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) ok = removeTree(path, depth + 1) && ok;
   else if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ok = RemoveDirectoryW(longPath(path).c_str()) && ok;
   else ok = DeleteFileW(longPath(path).c_str()) && ok;
  } while (FindNextFileW(h, &f));
  FindClose(h);
 }
 return RemoveDirectoryW(longPath(dir).c_str()) && ok;
}
// Older backups in `directory` are removed so only the newest `keep` complete ones stay. Only folders
// named saves-backup-<stamp> count (the stamp sorts by time), plus their -incomplete/-superseded set-asides,
// which were never usable as a backup. Anything else in the folder is never touched. Returns how many went.
inline constexpr size_t backupsKept = 2;
// `made`, the backup just taken, is never removed (a clock set back can give it an older stamp).
inline unsigned pruneBackups(const std::wstring& directory, const std::wstring& made = std::wstring(), size_t keep = backupsKept) {
 static const std::wstring prefix = L"saves-backup-";
 const size_t stampLength = 19;   // YYYYMMDD-HHMMSS-mmm, as stamp() writes it
 std::vector<std::wstring> complete, setAside;
 WIN32_FIND_DATAW f; HANDLE h = FindFirstFileW((longPath(directory) + L"*").c_str(), &f);
 if (h == INVALID_HANDLE_VALUE) return 0;
 do {
  const std::wstring name = f.cFileName;
  if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
  if (name.size() < prefix.size() + stampLength || name.compare(0, prefix.size(), prefix)) continue;
  bool stamped = true;
  for (size_t i = 0; i < stampLength; ++i) { const wchar_t c = name[prefix.size() + i]; stamped = stamped && ((i == 8 || i == 15) ? c == L'-' : (c >= L'0' && c <= L'9')); }
  if (!stamped) continue;
  const std::wstring rest = name.substr(prefix.size() + stampLength);
  if (rest.empty()) complete.push_back(name);
  else if (rest == L"-incomplete" || rest == L"-superseded") setAside.push_back(name);
 } while (FindNextFileW(h, &f));
 FindClose(h);
 std::sort(complete.begin(), complete.end(), std::greater<>());
 unsigned removed = 0;
 for (size_t i = keep; i < complete.size(); ++i) if (_wcsicmp((directory + complete[i]).c_str(), made.c_str())) removed += removeTree(directory + complete[i]);
 for (const std::wstring& name : setAside) removed += removeTree(directory + name);
 return removed;
}
// Restore: the backup is copied into a staging folder beside the saves (%APPDATA%\Exanima.restore-*),
// checked, and stillSafe() asked once more; then the save folder is renamed to
// Exanima.before-restore-* (kept whole) and the staging folder renamed into its place. If the game
// holds the save folder open (the rename is refused), the current folder is copied to
// Exanima.before-restore-* first and the files are put back one by one, game-made files moved aside.
// What happened, for the message: the saves are never left without a whole copy somewhere, and the
// message names where (kept) whenever the live folder is not what it was.
enum class Restored {
 done,           // the checked copy is the save folder; the old one is kept whole (kept)
 noBackup,       // nothing to restore from; nothing changed
 notPrepared,    // the checked copy could not be made; nothing changed
 notSafe,        // a game started loading; nothing changed
 putBack,        // the swap failed and the save folder was put back as it was; nothing changed
 savesInKept,    // the swap failed and the save folder could not be put back: the saves are whole in kept
 notStarted,     // file by file: the copy aside failed; nothing changed
 partial,        // file by file: some files could not be written; the saves as they were are whole in kept
};
inline Restored restore(const std::wstring& backupDir, unsigned& copied, unsigned& failed, std::wstring& kept, const std::function<bool()>& stillSafe) {
 const std::wstring to = folder(); copied = failed = 0; kept.clear();
 if (to.empty() || backupDir.empty() || !exists(backupDir)) return Restored::noBackup;
 std::vector<Entry> want, staged;
 if (!listTree(backupDir, want)) { failed = 1; return Restored::notPrepared; }
 const std::wstring stage = to + L".restore-" + stamp();
 copyTree(backupDir, stage, false, copied, failed);
 if (failed || !listTree(stage, staged) || !sameFiles(want, staged, false)) { if (!failed) failed = 1; return Restored::notPrepared; }
 if (stillSafe && !stillSafe()) { failed = 0; copied = 0; return Restored::notSafe; }
 kept = to + L".before-restore-" + stamp();
 if (!exists(to)) { kept.clear(); if (MoveFileExW(longPath(stage).c_str(), longPath(to).c_str(), 0)) return Restored::done; failed = 1; return Restored::putBack; }
 if (MoveFileExW(longPath(to).c_str(), longPath(kept).c_str(), 0)) {
  if (MoveFileExW(longPath(stage).c_str(), longPath(to).c_str(), 0)) return Restored::done;
  failed = 1;
  if (MoveFileExW(longPath(kept).c_str(), longPath(to).c_str(), 0)) { kept.clear(); return Restored::putBack; }   // the saves back as they were
  return Restored::savesInKept;                                     // something made a new save folder meanwhile: say where the saves are
 }
 // The game holds the folder open: a full copy aside first (if that is incomplete, nothing is restored).
 { unsigned c = 0, f = 0; copyTree(to, kept, false, c, f); if (f) { failed = f; return Restored::notStarted; } }
 unsigned moved = 0; copied = 0;
 moveNew(to, backupDir, kept + L"-game-written", moved, failed);
 copyTree(stage, to, true, copied, failed);
 return failed == 0 ? Restored::done : Restored::partial;
}

}  // namespace saves
