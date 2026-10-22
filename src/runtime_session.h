#pragma once
#include "script_host.h"
#include <limits>
namespace scripting {
struct ScriptSource {
 std::string id,source,required,optional;std::vector<Module> modules;size_t memory=8*1024*1024;   // Lua heap cap ([script] memory_mb)
 size_t bytes()const{size_t result=source.size();for(const auto& m:modules)result+=m.source.size();return result;}
};
// Called under the runtime mutex. Producers copy commands; only pulse consumes them.
class Session {
 struct Record {ScriptSource code;bool pending=true,waiting=false;};
 Host& host;std::vector<Record> records;
 uint64_t available=0;bool recoverable=false,settled=false,inside=false;uint64_t owner=0,generation=0,identity=0,menuSince=0,nextRetry=0;
 bool menuClock=false;std::string phase="loading",lastFailure;
 bool fail(const std::string& message){lastFailure=message;return false;}
 static constexpr size_t sourceLimit=16*1024*1024;
 bool validate(const ScriptSource& code){
  if(code.id.empty()||code.id.size()>128||code.bytes()>1024*1024||code.modules.size()>64)return fail("invalid addon ID or source bundle bounds");
  for(size_t i=0;i<code.modules.size();++i){
   if(!Host::validModuleName(code.modules[i].name))return fail("invalid module name");
   for(size_t j=0;j<i;++j)if(code.modules[j].name==code.modules[i].name)return fail("duplicate module name");
  }
  size_t begin=0;while(begin<code.required.size()){
   const auto end=code.required.find(',',begin);const auto name=code.required.substr(begin,end==std::string::npos?end:end-begin);
   const uint64_t bit=capability(name);if(!bit)return fail("unknown required capability: "+name);
   // Until the native API has started at a settled title menu, any known capability may still arrive: the addon
   // waits for it. After that (settled), a missing capability is final.
   if(!(available&bit)&&(!recoverable||settled))return fail("unavailable required capability: "+name);
   if(end==std::string::npos)break;begin=end+1;
  }return true;
 }
 bool ready(const ScriptSource& code)const{
  size_t begin=0;while(begin<code.required.size()){
   const auto end=code.required.find(',',begin);const auto bit=capability(code.required.substr(begin,end==std::string::npos?end:end-begin));
   if(!(available&bit))return false;if(end==std::string::npos)break;begin=end+1;
  }return true;
 }
 std::string missing(const ScriptSource& code)const{
  size_t begin=0;while(begin<code.required.size()){
   const auto end=code.required.find(',',begin);const auto name=code.required.substr(begin,end==std::string::npos?end:end-begin);
   if(!(available&capability(name)))return name;if(end==std::string::npos)break;begin=end+1;
  }return {};
 }
 bool refuse(Record& record){
  record.pending=false;record.waiting=false;lastFailure="unavailable required capability: "+missing(record.code)+
   " (this game build or another mod blocks it; see the runtime lines above)";
  host.report(record.code.id.c_str(),"error",lastFailure.c_str());return false;
 }
 bool apply(Record& record){
  if(!ready(record.code)){if(settled)return refuse(record);record.waiting=true;record.pending=false;return false;}
  record.pending=false;record.waiting=false;const auto& c=record.code;
  const bool loaded=host.load(c.id,c.source.data(),c.source.size(),c.required,c.memory,c.modules,c.optional);
  if(loaded)host.notifyPhase(phase,generation,c.id);else{lastFailure=host.error();host.report(c.id.c_str(),"error",lastFailure.c_str());}
  return loaded;
 }
public:
 explicit Session(Host& value):host(value){}
 void configure(uint64_t capabilities,bool canRecover){available=capabilities;recoverable=canRecover;host.capabilities(available);}
 // The native API has started: capabilities are final, so addons still waiting on a missing one are refused.
 void finish(){if(settled)return;settled=true;for(auto& record:records)if(record.waiting&&!ready(record.code))refuse(record);}
 const std::string& error()const{return lastFailure;}
 bool submit(const ScriptSource& code,bool queued){
  lastFailure.clear();if(!validate(code))return false;size_t total=code.bytes();
  for(const auto& record:records)if(record.code.id!=code.id)total+=record.code.bytes();
  if(total>sourceLimit)return fail("pending source catalog exceeds 16 MiB");
  for(auto& record:records)if(record.code.id==code.id){record={code,true,false};return queued||apply(record);}
  if(records.size()>=128)return fail("addon catalog exceeds 128 IDs");records.push_back({code,true,false});return queued||apply(records.back());
 }
 bool ownsThread(uint64_t thread)const{return owner&&owner==thread;}
 bool pending(const std::string& id)const{for(const auto& r:records)if(r.code.id==id)return r.pending||r.waiting;return false;}
 bool settledMenu(uint64_t now)const{return menuClock&&phase=="menu"&&now>=menuSince&&now-menuSince>=500;}
 bool retryDue(uint64_t now){if(!recoverable||!settledMenu(now)||now<nextRetry)return false;nextRetry=now+5000;return true;}
 // The caller supplies a window-owner thread proven by Win32, not an arbitrary first caller.
 bool begin(uint64_t thread,uint64_t windowThread,const std::string& observed,uint64_t player,uint64_t now){
  if(!thread||thread!=windowThread||(owner&&owner!=thread)||inside)return false;
  owner=thread;inside=true;host.setTime(now);
  const bool changed=observed!=phase||(observed=="session"&&player!=identity);
  if(observed=="session"&&player!=identity)++generation;
  identity=player;phase=observed;
  if(phase=="menu"){if(!menuClock){menuClock=true;menuSince=now;}}else menuClock=false;
  if(changed)host.notifyPhase(phase,generation);
  return true;
 }
 void consume(size_t budget=4){for(auto& record:records)if(record.pending||(record.waiting&&ready(record.code))){if(!budget--)break;apply(record);}}
 void tickTimers(){if(inside&&phase!="loading")host.runTimers();}
 void end(){inside=false;}
 void clear(){records.clear();host.clear();inside=false;owner=0;identity=0;phase="loading";menuClock=false;nextRetry=0;settled=false;}
};
}
