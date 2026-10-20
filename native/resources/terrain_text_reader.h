#pragma once
#include <streambuf>
#include <string_view>
namespace ae::resources::terrain_detail {
// Read-only stream buffer over caller-owned text: avoids duplicating a legal
// 32 MiB bitmap-hex payload while parsing an embedded FX record.
class TextBuffer final:public std::streambuf {
public:
  explicit TextBuffer(std::string_view text){static char empty=0;auto *p=text.empty()?&empty:const_cast<char*>(text.data());setg(p,p,p+text.size());}
  std::size_t offset()const{return std::size_t(gptr()-eback());}
  std::size_t remaining()const{return std::size_t(egptr()-gptr());}
};
}
