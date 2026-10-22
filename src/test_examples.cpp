#include "plugin.cpp"
#include <cstdio>
#include <regex>
#include "test_assert.h"
// Every shipped example loads as shipped, with all of its required and optional capabilities available and its
// declared modules, and logs no error or "unknown capability" warning. Disabled-by-default examples stay idle.
static std::vector<std::string> logged;
static void capture(void*,const char* id,const char* level,const char* message){logged.push_back(std::string(id)+"|"+level+"|"+message);}
static std::string read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);assert(file);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
// The manifest fields the loader reads: requires/optional arrays and [script.modules] name = "path" rows.
static std::string list(const std::string& toml,const char* key){
 const std::regex row(std::string("\\n")+key+"\\s*=\\s*\\[([^\\]]*)\\]");std::smatch m;if(!std::regex_search(toml,m,row))return "";
 std::string out;const std::regex name("\"([^\"]+)\"");const std::string body=m[1];
 for(auto i=std::sregex_iterator(body.begin(),body.end(),name);i!=std::sregex_iterator();++i){if(!out.empty())out+=',';out+=(*i)[1];}
 return out;
}
int main(){
 using namespace scripting;host.setLogger(capture);uint64_t all=0;for(const auto& known:capabilityNames)all|=known.bit;host.capabilities(all);
 unsigned examples=0;
 for(const auto& entry:std::filesystem::directory_iterator("examples")){
  const auto toml=read(entry.path()/"manifest.toml");std::smatch m;
  assert(std::regex_search(toml,m,std::regex("\\nentry\\s*=\\s*\"([^\"]+)\"")));const auto source=read(entry.path()/m[1].str());
  std::vector<Module> modules;const auto at=toml.find("[script.modules]");
  if(at!=std::string::npos){const std::string rows=toml.substr(at);const std::regex module("\\n\"?([A-Za-z0-9_.]+)\"?\\s*=\\s*\"([^\"]+)\"");
   for(auto i=std::sregex_iterator(rows.begin(),rows.end(),module);i!=std::sregex_iterator();++i)modules.push_back({(*i)[1],read(entry.path()/(*i)[2].str())});}
  const auto id=entry.path().filename().string();logged.clear();
  const bool loaded=host.load(id,source.data(),source.size(),list(toml,"requires"),8*1024*1024,modules,list(toml,"optional"));
  if(!loaded)std::fprintf(stderr,"%s: %s\n",id.c_str(),host.error().c_str());assert(loaded&&host.enabled(id));
  for(const auto& line:logged)if(line.find("|error|")!=std::string::npos||line.find("unknown capability")!=std::string::npos){std::fprintf(stderr,"%s\n",line.c_str());assert(false);}
  ++examples;
 }
 assert(examples>=10);std::printf("LUA_EXAMPLES_PASSED %u\n",examples);
}
