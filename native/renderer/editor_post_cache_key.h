#pragma once
#include "core/base.h"
#include <cstring>
#include <span>
#include <vector>
namespace ae::renderer {
constexpr u64 EditorPostCacheBudget=14ull*1024*1024;
struct EditorPostCacheKey {
  std::vector<u8> bytes;
  void append(const void *data,usize size){const auto *p=static_cast<const u8 *>(data);bytes.insert(bytes.end(),p,p+size);}
  template<class T> void value(const T &v){append(&v,sizeof(v));}
  bool operator==(const EditorPostCacheKey &)const=default;
};
inline bool editorPostCacheFits(u32 width,u32 height,u64 allocation=0){
  return width&&height&&u64(width)*height<=EditorPostCacheBudget/4&&(!allocation||allocation<=EditorPostCacheBudget);
}
}
