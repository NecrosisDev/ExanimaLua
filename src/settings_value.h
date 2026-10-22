#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace scripting::settings {
struct Value {
 enum Kind {null,boolean,integer,number,string,array,object} kind=null;
 bool flag=false;int64_t whole=0;double real=0;std::string text;std::vector<Value> list;std::map<std::string,Value> fields;
};
using Document=std::map<std::string,Value>;
inline bool utf8(const std::string& text){
 for(size_t i=0;i<text.size();){const auto c=static_cast<unsigned char>(text[i++]);if(c<128)continue;
  unsigned count=0;uint32_t point=0,minimum=0;
  if(c>=0xc2&&c<=0xdf){count=1;point=c&31;minimum=0x80;}
  else if(c>=0xe0&&c<=0xef){count=2;point=c&15;minimum=0x800;}
  else if(c>=0xf0&&c<=0xf4){count=3;point=c&7;minimum=0x10000;}else return false;
  if(count>text.size()-i)return false;
  while(count--){const auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80)return false;point=(point<<6)|(next&63);}
  if(point<minimum||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return false;
 }return true;
}
inline void appendPoint(std::string& result,uint32_t point){
 if(point<0x80)result+=char(point);
 else if(point<0x800){result+=char(0xc0|(point>>6));result+=char(0x80|(point&63));}
 else if(point<0x10000){result+=char(0xe0|(point>>12));result+=char(0x80|((point>>6)&63));result+=char(0x80|(point&63));}
 else{result+=char(0xf0|(point>>18));result+=char(0x80|((point>>12)&63));result+=char(0x80|((point>>6)&63));result+=char(0x80|(point&63));}
}
class Json {
 const std::string& source;size_t at=0,nodes=0;
 [[noreturn]] static void bad(){throw std::runtime_error("invalid or excessive settings JSON");}
 void space(){while(at<source.size()&&(source[at]==' '||source[at]=='\t'||source[at]=='\n'||source[at]=='\r'))++at;}
 bool take(char c){space();if(at<source.size()&&source[at]==c){++at;return true;}return false;}
 uint32_t hex(){uint32_t result=0;for(int i=0;i<4;++i){if(at==source.size())bad();const char c=source[at++];result<<=4;if(c>='0'&&c<='9')result+=c-'0';else if(c>='a'&&c<='f')result+=c-'a'+10;else if(c>='A'&&c<='F')result+=c-'A'+10;else bad();}return result;}
 std::string quoted(){
  if(!take('"'))bad();std::string result;
  while(at<source.size()){
   const unsigned char c=source[at++];if(c=='"'){if(!utf8(result))bad();return result;}if(c<32)bad();
   if(c!='\\'){result+=char(c);continue;}
   if(at==source.size())bad();const char escape=source[at++];
   switch(escape){case '"':case '\\':case '/':result+=escape;break;case 'b':result+='\b';break;case 'f':result+='\f';break;case 'n':result+='\n';break;case 'r':result+='\r';break;case 't':result+='\t';break;
    case 'u':{auto point=hex();if(point>=0xd800&&point<=0xdbff){if(at+2>source.size()||source[at++]!='\\'||source[at++]!='u')bad();const auto low=hex();if(low<0xdc00||low>0xdfff)bad();point=0x10000+((point-0xd800)<<10)+(low-0xdc00);}else if(point>=0xdc00&&point<=0xdfff)bad();appendPoint(result,point);break;}
    default:bad();
   }
  }bad();
 }
 Value value(size_t depth){
  if(depth>10||++nodes>520)bad();space();if(at==source.size())bad();Value result;
  if(source[at]=='"'){result.kind=Value::string;result.text=quoted();return result;}
  if(take('{')){result.kind=Value::object;if(take('}'))return result;do{auto key=quoted();if(!take(':'))bad();auto child=value(depth+1);if(!result.fields.emplace(std::move(key),std::move(child)).second)bad();if(take('}'))return result;}while(take(','));bad();}
  if(take('[')){result.kind=Value::array;if(take(']'))return result;do{result.list.push_back(value(depth+1));if(take(']'))return result;}while(take(','));bad();}
  for(const auto& literal:{std::string("true"),std::string("false"),std::string("null")})if(source.compare(at,literal.size(),literal)==0){at+=literal.size();result.kind=literal=="null"?Value::null:Value::boolean;result.flag=literal=="true";return result;}
  const size_t begin=at;if(source[at]=='-')++at;if(at==source.size())bad();
  if(source[at]=='0')++at;else{if(source[at]<'1'||source[at]>'9')bad();while(at<source.size()&&source[at]>='0'&&source[at]<='9')++at;}
  bool real=false;
  if(at<source.size()&&source[at]=='.'){real=true;++at;const size_t digits=at;while(at<source.size()&&source[at]>='0'&&source[at]<='9')++at;if(digits==at)bad();}
  if(at<source.size()&&(source[at]=='e'||source[at]=='E')){real=true;++at;if(at<source.size()&&(source[at]=='+'||source[at]=='-'))++at;const size_t digits=at;while(at<source.size()&&source[at]>='0'&&source[at]<='9')++at;if(digits==at)bad();}
  if(real){result.kind=Value::number;const auto parsed=std::from_chars(source.data()+begin,source.data()+at,result.real);if(parsed.ec!=std::errc()||parsed.ptr!=source.data()+at||!std::isfinite(result.real))bad();}
  else{result.kind=Value::integer;const auto parsed=std::from_chars(source.data()+begin,source.data()+at,result.whole);if(parsed.ec!=std::errc()||parsed.ptr!=source.data()+at)bad();}return result;
 }
public:
 explicit Json(const std::string& input):source(input){}
 Value parse(){if(source.size()>65536)bad();auto result=value(0);space();if(at!=source.size())bad();return result;}
};
inline void quote(std::string& result,const std::string& text){
 if(!utf8(text))throw std::runtime_error("settings strings must be valid UTF-8");
 static const char* digits="0123456789abcdef";result+='"';for(unsigned char c:text){if(c=='"'||c=='\\'){result+='\\';result+=char(c);}else if(c<32){result+="\\u00";result+=digits[c>>4];result+=digits[c&15];}else result+=char(c);}result+='"';
}
inline void encode(std::string& result,const Value& value,size_t depth,size_t& nodes){
 if(depth>10||++nodes>520)throw std::runtime_error("settings nesting/node limit exceeded");
 switch(value.kind){
  case Value::null:throw std::runtime_error("null is not a settings value");
  case Value::boolean:result+=value.flag?"true":"false";break;
  case Value::integer:result+=std::to_string(value.whole);break;
  case Value::number:{if(!std::isfinite(value.real))throw std::runtime_error("settings numbers must be finite");char buffer[64];auto encoded=std::to_chars(buffer,buffer+64,value.real,std::chars_format::general,17);if(encoded.ec!=std::errc())throw std::runtime_error("number encoding failed");std::string text(buffer,encoded.ptr);if(text.find_first_of(".eE")==std::string::npos)text+=".0";result+=text;break;}
  case Value::string:quote(result,value.text);break;
  case Value::array:{result+='[';bool first=true;for(const auto& child:value.list){if(!first)result+=',';first=false;encode(result,child,depth+1,nodes);}result+=']';break;}
  case Value::object:{result+='{';bool first=true;for(const auto& child:value.fields){if(child.first.empty()||child.first.size()>128)throw std::runtime_error("invalid settings object key");if(!first)result+=',';first=false;quote(result,child.first);result+=':';encode(result,child.second,depth+1,nodes);}result+='}';break;}
 }
 if(result.size()>65536)throw std::runtime_error("settings file exceeds 64 KiB");
}
inline std::string pack(const std::string& id,const Document& data){
 if(data.size()>128)throw std::runtime_error("settings key limit exceeded (128)");
 for(const auto& item:data)if(item.first.empty()||item.first.size()>64||item.first.find('\0')!=std::string::npos)throw std::runtime_error("invalid settings key");
 Value root;root.kind=Value::object;root.fields["format"].kind=Value::integer;root.fields["format"].whole=1;
 root.fields["addon"].kind=Value::string;root.fields["addon"].text=id;root.fields["values"].kind=Value::object;root.fields["values"].fields=data;
 std::string output;size_t nodes=0;encode(output,root,0,nodes);return output;
}
inline Document unpack(const std::string& id,const std::string& bytes){
 auto root=Json(bytes).parse();
 if(root.kind!=Value::object||root.fields.size()!=3||root.fields["format"].kind!=Value::integer||root.fields["format"].whole!=1||root.fields["addon"].kind!=Value::string||root.fields["addon"].text!=id||root.fields["values"].kind!=Value::object)throw std::runtime_error("settings schema or addon identity mismatch");
 pack(id,root.fields["values"].fields);return std::move(root.fields["values"].fields);
}
}
