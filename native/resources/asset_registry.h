#pragma once
#include "core/base.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

// Identidade de recurso: 128 bits, estável para sempre.
//
// **Não é o caminho e não é o hash.** Renomear ou mover muda o caminho e não a
// identidade; editar o arquivo muda o hash e não a identidade. Essa separação é
// a razão de o registro existir: uma cena que referencia `Malhas/porta.glb` por
// caminho quebra quando alguém arrasta o arquivo para outra pasta, e uma que
// referencia por hash quebra quando alguém corrige uma normal.
struct AssetGuid {
  u64 high = 0, low = 0;
  bool valid() const noexcept { return high != 0 || low != 0; }
  // 32 dígitos hexadecimais minúsculos, sem hífens: é o que vai para o arquivo
  // de cena e para o registro, e o que um humano compara de relance.
  std::string text() const;
  static bool parse(std::string_view text, AssetGuid &out);
  friend bool operator==(const AssetGuid &a, const AssetGuid &b) noexcept {
    return a.high == b.high && a.low == b.low;
  }
  friend bool operator!=(const AssetGuid &a, const AssetGuid &b) noexcept { return !(a == b); }
  friend bool operator<(const AssetGuid &a, const AssetGuid &b) noexcept {
    return a.high != b.high ? a.high < b.high : a.low < b.low;
  }
};
struct AssetGuidHash {
  usize operator()(const AssetGuid &guid) const noexcept { return static_cast<usize>(guid.high ^ (guid.low * 31)); }
};

// Derivado de uma semente por SHA-256. Determinístico de propósito: a migração
// de uma cena antiga precisa produzir hoje o mesmo GUID que produziu ontem,
// para o mesmo pacote e o mesmo índice de desenho.
AssetGuid assetGuidFromSeed(std::span<const u8> seed);
AssetGuid assetGuidFromSeed(std::string_view seed);

// Só tipos com implementação neste pacote. Um tipo registrado sem consumidor
// seria uma promessa no arquivo do usuário.
enum class AssetType : u32 {
  Mesh = 1,           // geometria instanciável
  Material = 2,       // parâmetros compartilhados (entrega C, ainda por ligar)
  Texture = 3,        // imagem cozida
  Script = 4,         // comportamento C# do projeto
  InputActionMap = 5, // mapa de ações do projeto
  Scene = 6,          // cena reutilizável
  EnvironmentProfile = 7, // aparência compartilhada por volumes de ambiente
  EnvironmentMap = 8 // panorama HDR linear, reflexão prefiltrada e irradiância SH9
};
const char *assetTypeName(AssetType type);
bool parseAssetType(std::string_view text, AssetType &out);

struct AssetRecord {
  AssetGuid guid;
  AssetType type = AssetType::Mesh;
  // Caminho relativo à raiz do projeto. Muda com renomear/mover.
  std::string path;
  // Arquivo de origem, quando o recurso veio de fora. Vazio para recursos que
  // nascem no projeto (um script criado pelo editor, por exemplo).
  std::string source;
  // SHA-256 do conteúdo da FONTE, em hexadecimal minúsculo. Vazio quando não há
  // fonte. É o que responde "mudou?" sem reimportar.
  std::string contentHash;
  // Versão do importador que produziu os derivados. Subir esta versão é o que
  // obriga uma reimportação mesmo com o hash igual.
  u32 importerVersion = 0;
  // Parâmetros do importador, texto opaco para o registro. Quem importa
  // interpreta; o registro só garante que eles voltam iguais.
  std::string importerParameters;
  // Outros recursos de que este depende (malha → material → textura).
  std::vector<AssetGuid> dependencies;
  // Arquivos produzidos pela importação, relativos à raiz. Apagar o recurso
  // apaga estes; a fonte importada é um deles quando foi copiada para dentro.
  std::vector<std::string> derived;
  bool valid() const;
};

// O registro do projeto. Uma lista só, com as regras que impedem duas verdades:
// GUID único, caminho único, dependência sempre para um GUID existente.
class AssetRegistry final {
public:
  static constexpr u32 FormatVersion = 1;
  static constexpr usize MaximumRecords = 65536;

  bool add(AssetRecord record);
  const AssetRecord *find(const AssetGuid &guid) const;
  const AssetRecord *findByPath(std::string_view path) const;
  // Renomear e mover são a MESMA operação: o caminho muda, o GUID não. Recusa
  // quando o destino já pertence a outro recurso.
  bool setPath(const AssetGuid &guid, std::string_view path);
  // Mover uma PASTA muda o caminho de tudo que está dentro dela. Devolve
  // quantos recursos foram reapontados, ou -1 quando a operação levaria dois
  // recursos ao mesmo caminho — e nesse caso nada muda.
  //
  // Uma operação só, e não uma sequência de `setPath`: aplicada aos poucos, uma
  // colisão no meio deixaria metade da pasta apontando para o lugar novo e
  // metade para o velho, e não há como saber qual metade.
  int retargetPrefix(std::string_view oldPrefix, std::string_view newPrefix);
  // Reimportação publicada: conteúdo, versão e derivados trocam de uma vez. Só
  // chame depois que a nova versão estiver íntegra — até aqui vale a anterior.
  bool publishImport(const AssetGuid &guid, std::string_view contentHash, u32 importerVersion,
                     std::string_view importerParameters, std::vector<std::string> derived,
                     std::vector<AssetGuid> dependencies);
  // Recusa enquanto alguém depender deste recurso, e diz quem.
  bool remove(const AssetGuid &guid);
  std::vector<AssetGuid> dependents(const AssetGuid &guid) const;

  usize size() const noexcept { return records_.size(); }
  const AssetRecord *at(usize index) const noexcept {
    return index < records_.size() ? &records_[index] : nullptr;
  }
  std::span<const AssetRecord> records() const noexcept { return records_; }

  std::string serialize() const;
  // Falha fechada: um registro inválido não vira um registro parcial. O chamador
  // fica com o que tinha.
  static bool deserialize(std::string_view text, AssetRegistry &out);

private:
  AssetRecord *edit(const AssetGuid &guid);
  std::vector<AssetRecord> records_;
};

} // namespace ae::resources
