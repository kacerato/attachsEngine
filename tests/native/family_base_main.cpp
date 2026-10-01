#include "harness.h"
#include <cstring>
int main(int argc,char **argv){
 unsigned failed=0,total=0;
 for(const auto &t:ae::test::registry()){
  if(argc>1&&!std::strstr(t.name,argv[1]))continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);failed+=ae::test::currentTestFailed();
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||!total?1:0;
}
