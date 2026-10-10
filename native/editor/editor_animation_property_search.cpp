#include "editor/editor_session.h"
#include <algorithm>
#include <cctype>

namespace ae::editor {
bool EditorSession::applyAnimationPropertySearch(std::string_view query) {
  if(query.size()>120||query.find('\0')!=std::string_view::npos)return false;
  state_.clipPropertyQuery=query;state_.clipPropertyPage=0;clipPropertyResourceRevision_=0;
  refreshAnimationPropertySurface();appearanceChanged_=true;return true;
}
bool animationPropertySearchMatches(const EditorScreenState::ClipPropertyEntry &entry,std::string_view query) {
  const auto lower=[](std::string text){for(auto &c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;};
  const auto haystack=lower(entry.label+" "+entry.address.type+" "+entry.address.property);const auto words=lower(std::string(query));
  usize start=0;
  while(start<words.size()) {
    while(start<words.size()&&std::isspace(static_cast<unsigned char>(words[start])))++start;
    const auto end=words.find_first_of(" \t\r\n",start);const auto word=words.substr(start,end==std::string::npos?end:end-start);
    if(!word.empty()&&haystack.find(word)==std::string::npos)return false;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return true;
}
}
