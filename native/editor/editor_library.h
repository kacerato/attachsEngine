#pragma once
#include "resources/library_index.h"
#include "editor/editor_import_transaction.h"
#include <functional>

namespace ae::editor {
inline constexpr const char *LibraryIndexPath=".astra/imports/library-index.astra";
struct LibrarySourceFile {std::string relative,sha256;u64 bytes=0;std::filesystem::file_time_type modified{};};
struct PreparedLibraryPackage {
  resources::LibraryPackage package;
  std::string project,staging,manifestPath,manifest,diagnostic;
  std::vector<LibrarySourceFile> files;
  std::vector<EditorImportTransaction::Companion> companions;
  bool accepted=false;
};
// Reads only a private staging directory, with cancellation and streaming
// hashes. Source preservation is independent from executable conversion.
PreparedLibraryPackage prepareLibraryPackage(const std::string &project,const std::string &staging,
    std::string name,std::vector<LibrarySourceFile> files,const std::function<bool()> &cancelled={});
}
