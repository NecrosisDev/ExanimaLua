#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <ctime>
namespace scripting {
// Bounded local diagnostics. Failure to write must never interrupt gameplay.
class FileLog {
 std::filesystem::path path;size_t limit;std::mutex mutex;
 static std::string clean(const char* source,size_t max){
  std::string result;if(!source)return result;
  for(size_t i=0;source[i]&&i<max;++i){const unsigned char c=source[i];
   if(c=='\n')result+="\\n";else if(c=='\r')continue;else if(c<32)result+=' ';else result+=char(c);
  }return result;
 }
public:
 explicit FileLog(std::filesystem::path file,size_t bytes=1024*1024):path(std::move(file)),limit(bytes){}
 void write(const char* id,const char* level,const char* message)noexcept{
  try{
   std::lock_guard<std::mutex> lock(mutex);std::error_code error;
   const auto line=std::to_string(std::time(nullptr))+" ["+clean(level,16)+"] ["+clean(id,128)+"] "+clean(message,4096)+"\n";
   const auto size=std::filesystem::file_size(path,error);
   if(!error&&(size>=limit||line.size()>limit-size)){
    auto backup=path;backup+=".previous";std::filesystem::remove(backup,error);error.clear();
    std::filesystem::rename(path,backup,error);if(error)return;
   }
   std::ofstream output(path,std::ios::binary|std::ios::app);output.write(line.data(),std::streamsize(line.size()));
  }catch(...){}
 }
};
}
