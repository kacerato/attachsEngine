#include "harness.h"
#include "scene/component_api_csharp.h"
#include "scene/component_reflection.h"
#include <fstream>
#include <cstring>
int writeFields2DProject(const char *);
int benchmarkFields2D();
int main(int argc,char **argv){
 if(argc==3&&std::strcmp(argv[1],"--write-project")==0)return writeFields2DProject(argv[2]);
 if(argc==2&&std::strcmp(argv[1],"--benchmark-fields")==0)return benchmarkFields2D();
 if(argc==3&&(std::strcmp(argv[1],"--write-component-api")==0||std::strcmp(argv[1],"--write-property-matrix")==0)){
  std::ofstream out(argv[2],std::ios::binary);if(!out)return 1;
  out<<(std::strcmp(argv[1],"--write-component-api")==0?ae::scene::componentCSharpApi():ae::scene::componentMatrixMarkdown());return out?0:1;
 }
 unsigned failed=0,total=0;
 for(const auto &t:ae::test::registry()){
  if(argc>1&&!std::strstr(t.name,argv[1]))continue;
  ++total;ae::test::currentTestFailed()=false;ae::test::currentTestName()=t.name;t.fn();
  std::printf("%s %s\n",ae::test::currentTestFailed()?"FAIL":"PASS",t.name);if(ae::test::currentTestFailed())++failed;
 }
 std::printf("%u/%u scenarios passed\n",total-failed,total);return failed||!total?1:0;
}
