#include "harness.h"
#include <string_view>
int main(int argc,char**argv){
 // Com argumento: qualquer cenário cujo nome contenha o trecho.
 const std::string_view filter=argc>1?argv[1]:"";
 unsigned failed=0,total=0;
 for(const auto&t:ae::test::registry()){
  const std::string_view name=t.name;
  if(filter.empty()?!name.starts_with("input_")&&!name.starts_with("android_game_input_"):name.find(filter)==std::string_view::npos)continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);failed+=ae::test::currentTestFailed();
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||!total?1:0;
}
