#include "saves.h"
#include <cstdio>
#include <filesystem>
#include <string>
#include "test_assert.h"
int main(){
 const auto root=std::filesystem::temp_directory_path()/("lua-saves-"+std::to_string(GetCurrentProcessId()));
 std::filesystem::create_directories(root/"hm_saves");
 const std::wstring appdata=saves::folder();assert(!appdata.empty()&&appdata.size()>8&&appdata.substr(appdata.size()-8)==L"\\Exanima");
 // Patched builds point the game's save folder elsewhere; backups follow it.
 std::string text=(root/"hm_saves").string()+"\\";const char* global=text.c_str();
 assert(saves::adoptGameFolder(uintptr_t(&global))&&saves::folder()==(root/"hm_saves").wstring());
 std::string missing=(root/"gone").string()+"\\";const char* other=missing.c_str();
 assert(!saves::adoptGameFolder(uintptr_t(&other))&&saves::folder()==(root/"hm_saves").wstring());
 const char* none=nullptr;assert(!saves::adoptGameFolder(uintptr_t(&none))&&!saves::adoptGameFolder(0));
 saves::gameFolder().clear();assert(saves::folder()==appdata);
 std::filesystem::remove_all(root);
 std::puts("LUA_SAVE_FOLDER_PASSED");
}
