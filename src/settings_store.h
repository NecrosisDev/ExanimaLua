#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include "settings_value.h"
namespace scripting::settings {
struct Revision {bool exists=false;std::string bytes;bool operator==(const Revision& other)const{return exists==other.exists&&bytes==other.bytes;}};
enum class Written {saved,conflict,error};
class FileStore {
 std::filesystem::path root;bool valid=false;
 struct Handle {HANDLE value=INVALID_HANDLE_VALUE;explicit Handle(HANDLE h):value(h){}~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;};
 static std::wstring native(const std::filesystem::path& path){
  auto text=path.wstring();if(text.rfind(L"\\\\?\\",0)==0)return text;
  if(text.rfind(L"\\\\",0)==0)return L"\\\\?\\UNC\\"+text.substr(2);
  return L"\\\\?\\"+text;
 }
 static bool plain(const std::filesystem::path& path,bool directory,bool missing){
  const auto attr=GetFileAttributesW(native(path).c_str());
  if(attr==INVALID_FILE_ATTRIBUTES)return missing&&(GetLastError()==ERROR_FILE_NOT_FOUND||GetLastError()==ERROR_PATH_NOT_FOUND);
  return !(attr&FILE_ATTRIBUTE_REPARSE_POINT)&&bool(attr&FILE_ATTRIBUTE_DIRECTORY)==directory;
 }
 static bool folder(const std::filesystem::path& path){
  if(CreateDirectoryW(native(path).c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS)return plain(path,true,false);return false;
 }
 bool directory(const std::string& id,std::filesystem::path& result)const{
  if(!valid||id.empty()||id.size()>128||!plain(root,true,false))return false;
  static const char* digits="0123456789abcdef";std::string encoded;
  for(unsigned char c:id){encoded+=digits[c>>4];encoded+=digits[c&15];}
  const auto first=root/encoded.substr(0,128);if(!folder(first))return false;
  result=first/(encoded.size()>128?encoded.substr(128):"_");return folder(result);
 }
 static bool readFile(const std::filesystem::path& path,Revision& revision){
  revision={};if(!plain(path,false,true))return false;
  Handle file(CreateFileW(native(path).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(file.value==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
  BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
  if(!GetFileInformationByHandle(file.value,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))||!GetFileSizeEx(file.value,&size)||size.QuadPart<0||size.QuadPart>65536)return false;
  revision.exists=true;revision.bytes.resize(size_t(size.QuadPart));DWORD got=0;
  return ReadFile(file.value,revision.bytes.data(),DWORD(revision.bytes.size()),&got,nullptr)&&got==revision.bytes.size();
 }
public:
 explicit FileStore(const std::filesystem::path& path){
  try{root=std::filesystem::absolute(path).lexically_normal();std::error_code error;std::filesystem::create_directories(root,error);valid=!error&&plain(root,true,false);}catch(...){valid=false;}
 }
 bool ready()const{return valid;}
 bool read(const std::string& id,Document& values,Revision& revision,std::string& error)const{
  error.clear();try{std::filesystem::path dir;if(!directory(id,dir)||!readFile(dir/L"settings.json",revision)){error="settings path or read failed";return false;}
   values=revision.exists?unpack(id,revision.bytes):Document{};return true;
  }catch(const std::exception& failure){error=failure.what();return false;}
 }
 Written write(const std::string& id,const Document& values,const Revision& expected,Revision& published,std::string& error)const{
  error.clear();try{
   std::filesystem::path dir;if(!directory(id,dir)){error="settings directory unavailable";return Written::error;}
   const auto current=dir/L"settings.json",temporary=dir/L"settings.tmp",previous=dir/L"settings.previous.json",lockPath=dir/L"settings.lock";
   if(!plain(lockPath,false,true)||!plain(temporary,false,true)||!plain(previous,false,true)){error="settings path is not a regular file";return Written::error;}
   Handle lock(CreateFileW(native(lockPath).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
   if(lock.value==INVALID_HANDLE_VALUE){error="settings writer is busy";return Written::error;}
   Revision actual;if(!readFile(current,actual)){error="settings revision read failed";return Written::error;}
   if(!(actual==expected)){error="settings changed outside this runtime; refusing to overwrite";return Written::conflict;}
   const auto bytes=pack(id,values);
   {
    Handle file(CreateFileW(native(temporary).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));DWORD written=0;
    if(file.value==INVALID_HANDLE_VALUE||!WriteFile(file.value,bytes.data(),DWORD(bytes.size()),&written,nullptr)||written!=bytes.size()||!FlushFileBuffers(file.value)){error="settings temporary write failed";return Written::error;}
   }
   const bool done=actual.exists?ReplaceFileW(native(current).c_str(),native(temporary).c_str(),native(previous).c_str(),0,nullptr,nullptr)!=0:
       MoveFileExW(native(temporary).c_str(),native(current).c_str(),MOVEFILE_WRITE_THROUGH)!=0;
   if(!done){error="settings atomic publication failed";return Written::error;}
   published={true,bytes};return Written::saved;
  }catch(const std::exception& failure){error=failure.what();return Written::error;}
 }
};
struct Cache {Document values;Revision revision;bool dirty=false,conflict=false;uint64_t retry=0;std::string error;};
}
