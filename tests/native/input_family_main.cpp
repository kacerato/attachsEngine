#include "harness.h"
#include <string_view>
int main(){
 unsigned failed=0,total=0;
 for(const auto&t:ae::test::registry()){
  const std::string_view name=t.name;
  if(!name.starts_with("input_")&&!name.starts_with("android_game_input_"))continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);failed+=ae::test::currentTestFailed();
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||!total?1:0;
}
