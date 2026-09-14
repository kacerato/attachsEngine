#include "harness.h"

#include <cstdio>
#include <cstring>
int inspectGlbFile(const char *path);
int probeReimportGlb(const char *path);
int writeM082Fixtures(const char *directory);
int probePackGltf(int count, char **paths);

int main(int argc,char **argv) {
  if(argc==3 && std::strcmp(argv[1],"--import-glb")==0) return inspectGlbFile(argv[2]);
  if(argc==3 && std::strcmp(argv[1],"--reimport-glb")==0) return probeReimportGlb(argv[2]);
  if(argc==3 && std::strcmp(argv[1],"--write-m082-fixtures")==0) return writeM082Fixtures(argv[2]);
  // --pack-gltf <principal> [companheiros...]: empacota como o seletor faria e importa o resultado.
  if(argc>=3 && std::strcmp(argv[1],"--pack-gltf")==0) return probePackGltf(argc-2,argv+2);
  int failCount = 0;
  int total = 0;
  for (const auto &tc : ae::test::registry()) {
    if(argc>1 && !std::strstr(tc.name,argv[1])) continue;
    ae::test::currentTestFailed() = false;
    ae::test::currentTestName() = tc.name;
    tc.fn();
    ++total;
    if (ae::test::currentTestFailed()) {
      ++failCount;
      std::fprintf(stderr, "[FALHOU] %s\n", tc.name);
    } else {
      std::printf("[ok] %s\n", tc.name);
    }
  }
  std::printf("\n%d/%d testes passaram\n", total - failCount, total);
  return failCount == 0 ? 0 : 1;
}
