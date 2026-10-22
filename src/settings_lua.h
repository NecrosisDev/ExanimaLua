#pragma once
#include "settings_value.h"
namespace scripting::settings {
inline Value fromLua(lua_State* L,int index,size_t depth,size_t& nodes,std::vector<const void*>& active){
 if(depth>8||++nodes>512)throw std::runtime_error("settings nesting/node limit exceeded");
 index=lua_absindex(L,index);Value value;
 switch(lua_type(L,index)){
  case LUA_TBOOLEAN:value.kind=Value::boolean;value.flag=lua_toboolean(L,index)!=0;return value;
  case LUA_TNUMBER:if(lua_isinteger(L,index)){value.kind=Value::integer;value.whole=lua_tointeger(L,index);}else{value.kind=Value::number;value.real=lua_tonumber(L,index);if(!std::isfinite(value.real))throw std::runtime_error("settings numbers must be finite");}return value;
  case LUA_TSTRING:{size_t size=0;const char* text=lua_tolstring(L,index,&size);if(size>65536)throw std::runtime_error("settings string too large");value.kind=Value::string;value.text.assign(text,size);if(!utf8(value.text))throw std::runtime_error("settings strings must be UTF-8");return value;}
  case LUA_TTABLE:{
   const void* identity=lua_topointer(L,index);for(auto pointer:active)if(pointer==identity)throw std::runtime_error("cyclic settings table");active.push_back(identity);
   std::map<int64_t,Value> array;value.kind=Value::object;
   if(!lua_checkstack(L,4))throw std::runtime_error("settings stack exhausted");lua_pushnil(L);
   while(lua_next(L,index)){
    auto child=fromLua(L,-1,depth+1,nodes,active);
    if(lua_type(L,-2)==LUA_TSTRING){size_t size=0;const char* key=lua_tolstring(L,-2,&size);if(!size||size>128)throw std::runtime_error("invalid settings table key");value.fields.emplace(std::string(key,size),std::move(child));}
    else if(lua_isinteger(L,-2)&&lua_tointeger(L,-2)>0)array.emplace(lua_tointeger(L,-2),std::move(child));
    else throw std::runtime_error("settings table keys must be strings or dense positive integers");
    lua_pop(L,1);
   }
   active.pop_back();
   if(!array.empty()){
    if(!value.fields.empty()||array.rbegin()->first!=static_cast<int64_t>(array.size()))throw std::runtime_error("mixed or sparse settings array");
    value.kind=Value::array;for(auto& child:array)value.list.push_back(std::move(child.second));
   }return value;
  }
  default:throw std::runtime_error("unsupported settings value type");
 }
}
inline void toLua(lua_State* L,const Value& value){
 if(!lua_checkstack(L,4))throw std::runtime_error("settings stack exhausted");
 switch(value.kind){
  case Value::boolean:lua_pushboolean(L,value.flag);break;
  case Value::integer:lua_pushinteger(L,value.whole);break;
  case Value::number:lua_pushnumber(L,value.real);break;
  case Value::string:lua_pushlstring(L,value.text.data(),value.text.size());break;
  case Value::array:lua_createtable(L,int(value.list.size()),0);for(size_t i=0;i<value.list.size();++i){toLua(L,value.list[i]);lua_rawseti(L,-2,static_cast<lua_Integer>(i+1));}break;
  case Value::object:lua_createtable(L,0,int(value.fields.size()));for(const auto& child:value.fields){lua_pushlstring(L,child.first.data(),child.first.size());toLua(L,child.second);lua_rawset(L,-3);}break;
  default:throw std::runtime_error("unsupported stored settings type");
 }
}
}
