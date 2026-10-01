#include "harness.h"
#include "scene/component_api_csharp.h"
#include <fstream>
#include <iterator>
#include <string_view>
int main(int argc,char**argv){
 if(argc==3&&std::string_view(argv[1])=="--write-component-api"){
  std::ofstream file(argv[2],std::ios::binary);file<<ae::scene::componentCSharpApi();return file.good()?0:1;
 }
 std::ifstream file(std::string(AETHER_REPOSITORY_ROOT)+"/managed/Astra.Scripting/Generated/Components.g.cs",std::ios::binary);
 const std::string bytes(std::istreambuf_iterator<char>(file),{});std::string normalized;for(char c:bytes)if(c!='\r')normalized+=c;
 if(!file||normalized!=ae::scene::componentCSharpApi()){std::puts("Generated SDK does not match schema; use --write-component-api");return 1;}
 unsigned failed=0,total=0;
 for(const auto&t:ae::test::registry()){
  const std::string_view name=t.name;if(!name.starts_with("curve3d_")&&!name.starts_with("path_")&&!name.starts_with("paths_"))continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);failed+=ae::test::currentTestFailed();
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||total!=13?1:0;
}
