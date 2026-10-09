#pragma once
#include <string>
#include <string_view>

namespace ae::resources {
// Canonical scene paths, not filesystem paths. Encode reserved bytes so an
// object named "A/B", "..", or "50%" remains a literal object name.
inline std::string animationBindingSegment(std::string_view name) {
  constexpr char hex[]="0123456789ABCDEF";std::string result;
  for(unsigned char c:name) {
    if(c=='%'||c=='/'||c=='\\'||c<32||c==127||((name=="."||name=="..")&&c=='.')) {
      result+='%';result+=hex[c>>4];result+=hex[c&15];
    } else result+=static_cast<char>(c);
  }
  return result;
}
inline bool animationBindingName(std::string_view encoded,std::string &out) {
  if(encoded.empty()||encoded=="."||encoded=="..")return false;
  const auto digit=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;};
  std::string result;
  for(size_t i=0;i<encoded.size();++i) {
    const char c=encoded[i];
    if(c=='%') {
      if(i+2>=encoded.size())return false;
      const int a=digit(encoded[i+1]),b=digit(encoded[i+2]);
      if(a<0||b<0)return false;
      result+=static_cast<char>((a<<4)|b);i+=2;
    } else result+=c;
  }
  if(animationBindingSegment(result)!=encoded)return false;
  out=std::move(result);return true;
}
inline std::string migrateAnimationBindingPath(std::string_view legacy) {
  std::string result;
  for(size_t start=0;start<legacy.size();) {
    const auto end=legacy.find('/',start);if(!result.empty())result+='/';
    result+=animationBindingSegment(legacy.substr(start,end==legacy.npos?legacy.size()-start:end-start));
    if(end==legacy.npos)break;
    start=end+1;
  }
  return result;
}
} // namespace ae::resources
