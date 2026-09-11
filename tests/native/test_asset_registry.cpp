#include "harness.h"
#include "core/sha256.h"
#include "resources/asset_registry.h"

#include <string>
#include <vector>

using namespace ae;
using namespace ae::resources;

namespace {
AssetRecord meshRecord(const char *seed, const char *path) {
  AssetRecord record;
  record.guid = assetGuidFromSeed(seed);
  record.type = AssetType::Mesh;
  record.path = path;
  return record;
}
} // namespace

AE_TEST(sha256_matches_the_published_vectors) {
  // Sem vetores conhecidos, uma implementação de hash "funciona" até o dia em
  // que dois arquivos iguais têm hashes diferentes em plataformas diferentes.
  AE_EXPECT_TRUE(Sha256::hex(std::span<const u8>()) ==
                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                 "string vazia");
  const std::string abc = "abc";
  AE_EXPECT_TRUE(Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(abc.data()), abc.size())) ==
                     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                 "abc");
  // 56 bytes força o bloco extra de padding — é onde uma implementação errada
  // quase sempre quebra.
  const std::string longer = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  AE_EXPECT_EQ(longer.size(), 56u, "tamanho da borda de padding");
  AE_EXPECT_TRUE(Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(longer.data()), longer.size())) ==
                     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                 "56 bytes");
}

AE_TEST(asset_guid_round_trips_and_refuses_garbage) {
  const auto guid = assetGuidFromSeed("pacote:7:3");
  AE_EXPECT_TRUE(guid.valid(), "semente produz GUID válido");
  AE_EXPECT_EQ(guid.text().size(), 32u, "32 dígitos hexadecimais");
  AssetGuid parsed;
  AE_EXPECT_TRUE(AssetGuid::parse(guid.text(), parsed), "ida e volta");
  AE_EXPECT_TRUE(parsed == guid, "mesmo valor");
  // A mesma semente precisa dar o mesmo GUID hoje e amanhã: é disto que a
  // migração de uma cena antiga depende.
  AE_EXPECT_TRUE(assetGuidFromSeed("pacote:7:3") == guid, "determinístico");
  AE_EXPECT_TRUE(!(assetGuidFromSeed("pacote:7:4") == guid), "sementes diferentes, GUIDs diferentes");
  AE_EXPECT_TRUE(!AssetGuid::parse("", parsed), "vazio recusado");
  AE_EXPECT_TRUE(!AssetGuid::parse(std::string(32, '0'), parsed), "zero não é identidade");
  AE_EXPECT_TRUE(!AssetGuid::parse(std::string(31, 'a'), parsed), "comprimento errado recusado");
  AE_EXPECT_TRUE(!AssetGuid::parse(std::string(32, 'A'), parsed), "maiúsculas recusadas");
}

AE_TEST(asset_identity_survives_rename_and_content_change) {
  AssetRegistry registry;
  auto record = meshRecord("import:porta", "Malhas/porta.mesh");
  record.source = "Fontes/porta.glb";
  record.contentHash = std::string(64, 'a');
  record.importerVersion = 1;
  const auto guid = record.guid;
  AE_EXPECT_TRUE(registry.add(std::move(record)), "recurso registrado");

  // Renomear e mover são a mesma coisa e NÃO criam outra identidade.
  AE_EXPECT_TRUE(registry.setPath(guid, "Cenario/Portas/porta-principal.mesh"), "renomear/mover");
  AE_EXPECT_TRUE(registry.find(guid) != nullptr, "identidade preservada");
  AE_EXPECT_TRUE(registry.find(guid)->path == "Cenario/Portas/porta-principal.mesh", "caminho novo");
  AE_EXPECT_TRUE(registry.findByPath("Malhas/porta.mesh") == nullptr, "caminho antigo não resolve mais");

  // Editar o conteúdo muda o hash e NÃO cria outra identidade.
  AE_EXPECT_TRUE(registry.publishImport(guid, std::string(64, 'b'), 2, "escala=1", {"Cenario/Portas/porta-principal.mesh"}, {}),
                 "reimportação publicada");
  AE_EXPECT_TRUE(registry.find(guid)->contentHash == std::string(64, 'b'), "hash novo");
  AE_EXPECT_EQ(registry.find(guid)->importerVersion, 2u, "versão do importador nova");
  AE_EXPECT_TRUE(registry.find(guid)->guid == guid, "identidade preservada de novo");

  // Caminho ocupado por outro recurso é recusado: dois recursos no mesmo lugar
  // fariam a resolução por caminho depender da ordem da lista.
  AE_EXPECT_TRUE(registry.add(meshRecord("import:janela", "Malhas/janela.mesh")), "segundo recurso");
  AE_EXPECT_TRUE(!registry.setPath(guid, "Malhas/janela.mesh"), "colisão de caminho recusada");
}

AE_TEST(asset_registry_refuses_dangling_dependencies_and_protects_dependents) {
  AssetRegistry registry;
  auto texture = meshRecord("tex", "Texturas/madeira.tex");
  texture.type = AssetType::Texture;
  const auto textureGuid = texture.guid;
  AE_EXPECT_TRUE(registry.add(std::move(texture)), "textura");

  auto material = meshRecord("mat", "Materiais/madeira.mat");
  material.type = AssetType::Material;
  material.dependencies.push_back(textureGuid);
  const auto materialGuid = material.guid;
  AE_EXPECT_TRUE(registry.add(std::move(material)), "material depende da textura");

  // Dependência para um GUID que não existe é referência pendurada com cara de
  // válida: recusada na entrada, não descoberta na hora de carregar.
  auto quebrado = meshRecord("quebrado", "Malhas/quebrado.mesh");
  quebrado.dependencies.push_back(assetGuidFromSeed("inexistente"));
  AE_EXPECT_TRUE(!registry.add(std::move(quebrado)), "dependência inexistente recusada");

  const auto dependentes = registry.dependents(textureGuid);
  AE_EXPECT_EQ(dependentes.size(), 1u, "um dependente");
  AE_EXPECT_TRUE(dependentes[0] == materialGuid, "o material é o dependente");
  AE_EXPECT_TRUE(!registry.remove(textureGuid), "apagar recurso com dependente é recusado");
  AE_EXPECT_TRUE(registry.remove(materialGuid), "apagar o dependente primeiro");
  AE_EXPECT_TRUE(registry.remove(textureGuid), "agora a textura sai");
  AE_EXPECT_EQ(registry.size(), 0u, "registro vazio");
}

AE_TEST(asset_registry_round_trips_and_fails_closed) {
  AssetRegistry registry;
  auto texture = meshRecord("tex", "Texturas/madeira.tex");
  texture.type = AssetType::Texture;
  texture.source = "Fontes/madeira.png";
  texture.contentHash = std::string(64, 'c');
  const auto textureGuid = texture.guid;
  AE_EXPECT_TRUE(registry.add(std::move(texture)), "textura");
  auto mesh = meshRecord("malha", "Malhas/porta.mesh");
  mesh.dependencies.push_back(textureGuid);
  mesh.derived.push_back("Malhas/porta.mesh");
  mesh.derived.push_back("Malhas/porta.bin");
  mesh.importerParameters = "escala=0.01 eixo=Y";
  AE_EXPECT_TRUE(registry.add(std::move(mesh)), "malha");

  const auto text = registry.serialize();
  AssetRegistry back;
  AE_EXPECT_TRUE(AssetRegistry::deserialize(text, back), "releitura");
  AE_EXPECT_EQ(back.size(), 2u, "dois recursos");
  const auto *restored = back.findByPath("Malhas/porta.mesh");
  AE_EXPECT_TRUE(restored != nullptr, "malha encontrada pelo caminho");
  AE_EXPECT_EQ(restored->dependencies.size(), 1u, "dependência preservada");
  AE_EXPECT_TRUE(restored->dependencies[0] == textureGuid, "dependência é a textura");
  AE_EXPECT_EQ(restored->derived.size(), 2u, "derivados preservados");
  AE_EXPECT_TRUE(restored->importerParameters == "escala=0.01 eixo=Y", "parâmetros do importador");
  AE_EXPECT_TRUE(back.serialize() == text, "ida e volta estável");

  // Falha fechada: o registro de destino não pode ficar meio preenchido.
  AssetRegistry survivor;
  AE_EXPECT_TRUE(AssetRegistry::deserialize(text, survivor), "carga válida");
  AE_EXPECT_TRUE(!AssetRegistry::deserialize("AETHER_ASSETS 1 2\n", survivor), "contagem sem registros");
  AE_EXPECT_EQ(survivor.size(), 2u, "o registro anterior sobrevive à carga inválida");
  AE_EXPECT_TRUE(!AssetRegistry::deserialize("AETHER_ASSETS 99 0\n", survivor), "versão futura recusada");
  AE_EXPECT_TRUE(!AssetRegistry::deserialize("", survivor), "vazio recusado");
}

AE_TEST(asset_paths_cannot_escape_the_project) {
  AssetRegistry registry;
  // Um caminho que sobe de diretório, absoluto ou com drive apontaria para fora
  // do projeto — e o registro é justamente o que promete que tudo mora dentro.
  for (const char *path : {"../fora.mesh", "/etc/passwd", "C:/temp/x.mesh", "Malhas//porta.mesh",
                           "Malhas/../../fora.mesh", "Malhas/porta.mesh/", ".oculto/porta.mesh"}) {
    auto record = meshRecord(path, path);
    AE_EXPECT_TRUE(!registry.add(std::move(record)), path);
  }
  AE_EXPECT_EQ(registry.size(), 0u, "nada entrou");
  AE_EXPECT_TRUE(registry.add(meshRecord("ok", "Malhas/sub pasta/porta 1.mesh")),
                 "espaço e subpasta continuam válidos");
}
