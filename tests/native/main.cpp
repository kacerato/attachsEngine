#include "harness.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
int inspectGlbFile(const char *path);
int probeReimportGlb(const char *path);
int writeM082Fixtures(const char *directory);
int probePackGltf(int count, char **paths);
int probeImportGltfFolder(const char *mainPath, unsigned compression, bool lods);
int probeFolderPublish(const char *mainPath);
int writePropertyMatrix(const char *path);
int writeComponentApi(const char *path);
int printSourceReport(const char *path);

int main(int argc,char **argv) {
  if(argc==3 && std::strcmp(argv[1],"--import-glb")==0) return inspectGlbFile(argv[2]);
  if(argc==3 && std::strcmp(argv[1],"--reimport-glb")==0) return probeReimportGlb(argv[2]);
  if(argc==3 && std::strcmp(argv[1],"--write-m082-fixtures")==0) return writeM082Fixtures(argv[2]);
  // --pack-gltf <principal> [companheiros...]: empacota como o seletor faria e importa o resultado.
  if(argc>=3 && std::strcmp(argv[1],"--pack-gltf")==0) return probePackGltf(argc-2,argv+2);
  // --import-gltf-folder <principal.gltf>: fonte em pasta (S0), buffers e imagens lidos do disco.
  // [compressão]: 0 RGBA8, 4/6/8 ASTC NxN (S1).
  // [lod]: gera LOD e ordem de polígonos (S3), como o preset de fonte nova.
  if((argc>=3&&argc<=5) && std::strcmp(argv[1],"--import-gltf-folder")==0)
    return probeImportGltfFolder(argv[2],argc>=4?static_cast<unsigned>(std::atoi(argv[3])):0u,
                                 argc==5 && std::strcmp(argv[4],"lod")==0);
  // --publish-gltf-folder <principal.gltf>: custo de cada etapa da publicação na thread do editor.
  if(argc==3 && std::strcmp(argv[1],"--publish-gltf-folder")==0) return probeFolderPublish(argv[2]);
  // Regenera a matriz de propriedades a partir dos descritores de componente.
  if(argc==3 && std::strcmp(argv[1],"--write-property-matrix")==0) return writePropertyMatrix(argv[2]);
  // Regenera a fachada C# tipada (managed/Astra.Scripting/Generated/Components.g.cs).
  if(argc==3 && std::strcmp(argv[1],"--write-component-api")==0) return writeComponentApi(argv[2]);
  // Relatorio da fonte (G6-A) no terminal: os mesmos numeros que a aba Malhas
  // mostra no aparelho, para conferir um arquivo sem abrir o editor.
  if(argc==3 && std::strcmp(argv[1],"--source-report")==0) return printSourceReport(argv[2]);
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
