#include "harness.h"
#include <string_view>
int main(){unsigned failed=0,total=0;
 for(const auto&t:ae::test::registry()){
  const std::string_view name=t.name;
  if(!name.starts_with("query_contract_")&&!name.starts_with("runtime_raycast_")&&!name.starts_with("runtime_queries_")&&name!="runtime_shape_cast_and_overlap_find_obstacles_for_a_camera"&&name!="runtime_last_collider_disable_keeps_motion_and_removes_queries_until_reenabled")continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);failed+=ae::test::currentTestFailed();
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||!total?1:0;
}
