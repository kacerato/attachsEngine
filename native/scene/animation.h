#pragma once
#include "resources/asset_registry.h"
#include "resources/skeletal_animation.h"
#include "scene/components.h"

#include <array>
#include <cmath>
#include <vector>

namespace ae::scene {
// O componente Animation (legado) da Unity: a lista de clipes que este objeto
// pode tocar (`animations`), o clipe padrão (`clip`), Play Automatically e o
// Wrap Mode padrão dos estados.
//
// Os clipes são RECURSOS (resources::AssetType::AnimationClip), endereçados por
// identidade derivada da fonte importada e do nome do clipe — reordenar os
// clipes no arquivo não troca o que toca. O componente guarda só o que é
// autoral. Tocar, parar, misturar, tempo, velocidade e peso de cada clipe são
// estado de EXECUÇÃO e moram no avaliador (runtime/scene_animation.h), que os
// scripts comandam pela API de animação; nada disso vai para o arquivo.
//
// Os canais encontram os nós pela identidade do nó importado abaixo deste
// objeto e, quando não há vínculo, pelo nome: um clipe de uma fonte anima uma
// cópia da mesma hierarquia (retarget por nome, como o caminho da Unity).
class Animation final : public ComponentValue {
public:
  static constexpr usize MaximumClips = 32;
  struct ClipEntry {
    u64 id=0; // identidade do elemento, independente da posição e do AssetGuid
    resources::AssetGuid asset{};
    bool operator==(const ClipEntry &) const = default;
  };
  std::vector<ClipEntry> clips;
  resources::AssetGuid clip{};
  bool enabled = true;
  bool playAutomatically = true;
  // glTF não declara modo de repetição; Loop deixa o clipe importado visível.
  resources::AnimationWrapMode wrapMode = resources::AnimationWrapMode::Loop;
  // Velocidade inicial dos estados (AnimationState.speed de cada clipe).
  float speed = 1;
  // v1 guardava o clipe por índice na fonte. A carga preserva o índice até o
  // editor, que conhece a fonte, trocá-lo pela identidade (nunca gravado).
  u32 legacyClipIndex = ~0u;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Animation>(*this); }
  bool valid() const override {
    if(clips.size()>MaximumClips || static_cast<u32>(wrapMode)>3 || !std::isfinite(speed) || speed<-10 || speed>10 ||
       !nextClipId_) return false;
    for(usize i=0;i<clips.size();++i) {
      if(!clips[i].id || clips[i].id>=nextClipId_) return false;
      for(usize j=0;j<i;++j) if(clips[i].id==clips[j].id) return false;
    }
    return true;
  }
  bool contains(const resources::AssetGuid &id) const {
    for (const auto &entry : clips) if (entry.asset == id) return true;
    return false;
  }
  u64 appendClip(resources::AssetGuid asset={}) {
    if(clips.size()>=MaximumClips || nextClipId_==std::numeric_limits<u64>::max()) return 0;
    const u64 id=nextClipId_++;
    clips.push_back({id,asset});
    return id;
  }
  bool resizeClips(usize count) {
    if(count>MaximumClips || (count>clips.size() && count-clips.size()>std::numeric_limits<u64>::max()-nextClipId_)) return false;
    if(count<clips.size()) clips.resize(count);
    else while(clips.size()<count) if(!appendClip()) return false;
    return true;
  }
  bool moveClip(u64 id,usize target) {
    if(target>=clips.size()) return false;
    usize from=0;while(from<clips.size() && clips[from].id!=id) ++from;
    if(from==clips.size()) return false;
    const auto entry=clips[from];clips.erase(clips.begin()+static_cast<std::ptrdiff_t>(from));
    clips.insert(clips.begin()+static_cast<std::ptrdiff_t>(target),entry);
    return true;
  }
  bool removeClip(u64 id) {
    for(auto it=clips.begin();it!=clips.end();++it) if(it->id==id) {clips.erase(it);return true;}
    return false;
  }
  u64 nextClipId() const noexcept {return nextClipId_;}
  bool reserveClipIdsUntil(u64 floor) {if(!floor)return false;nextClipId_=std::max(nextClipId_,floor);return valid();}
  void write(std::ostream &out) const override {
    const auto guid = [](const resources::AssetGuid &value) { return value.valid() ? value.text() : std::string("-"); };
    out << guid(clip) << ' ' << playAutomatically << ' ' << static_cast<u32>(wrapMode) << ' ' << speed << ' '
        << clips.size() << ' ' << nextClipId_;
    for (const auto &entry : clips) out << ' ' << entry.id << ' ' << guid(entry.asset);
    out << ' ' << enabled;
  }
  bool read(std::istream &in, u32 version) override {
    enabled = true;
    clips.clear();
    nextClipId_=1;
    clip = {};
    legacyClipIndex = ~0u;
    u32 mode = 0;
    if (version == 1) {
      // v1: índice, tocar ao iniciar, repetição, velocidade.
      u32 index = 0;
      if (!(in >> index >> playAutomatically >> mode >> speed) || mode > 3) return false;
      legacyClipIndex = index;
      wrapMode = static_cast<resources::AnimationWrapMode>(mode);
      return valid();
    }
    if (version < 2 || version > 4) return false;
    std::string clipText;
    usize count = 0;
    if (!(in >> clipText >> playAutomatically >> mode >> speed >> count) || mode > 3 || count > MaximumClips) return false;
    const auto parse = [](const std::string &text, resources::AssetGuid &out) {
      out = {};
      return text == "-" || resources::AssetGuid::parse(text, out);
    };
    if (!parse(clipText, clip)) return false;
    if(version>=3 && !(in>>nextClipId_)) return false;
    for (usize i=0;i<count;++i) {
      std::string text;
      u64 id=0;
      if(version>=3 && !(in>>id)) return false;
      if (!(in >> text)) return false;
      resources::AssetGuid asset;
      if(!parse(text,asset)) return false;
      if(version==2) {if(!appendClip(asset)) return false;}
      else clips.push_back({id,asset});
    }
    if(version>=4 && !(in>>enabled)) return false;
    wrapMode = static_cast<resources::AnimationWrapMode>(mode);
    return valid();
  }
private:
  u64 nextClipId_=1;
};

inline constexpr std::array<ComponentNumber, 1> animationNumbers{{
  {"Velocidade", -10, 10, .05f,
   [](const ComponentValue &v) -> const float & { return static_cast<const Animation &>(v).speed; },
   [](ComponentValue &v) -> float * { return &static_cast<Animation &>(v).speed; }, "speed",
   {"Reprodução", "x", "Velocidade inicial de cada clipe (AnimationState.speed); negativo toca de trás para frente"}}
}};
inline constexpr std::array<ComponentBoolean, 2> animationBooleans{{
  {"enabled", "Ativa",
   [](const ComponentValue &v) { return static_cast<const Animation &>(v).enabled; },
   [](ComponentValue &v, bool value) { static_cast<Animation &>(v).enabled = value; },
   {"Reprodução", "", "Desligar suspende tempo e avaliação; religar retoma os estados"}},
  {"play_automatically", "Tocar ao iniciar",
   [](const ComponentValue &v) { return static_cast<const Animation &>(v).playAutomatically; },
   [](ComponentValue &v, bool value) { static_cast<Animation &>(v).playAutomatically = value; },
   {"Reprodução", "", "Play Automatically: o clipe padrão começa a tocar quando o Play inicia"}}
}};
inline constexpr std::array<ComponentEnumOption, 4> animationWrapOptions{{
  {0, "Uma vez"}, {1, "Repetir"}, {2, "Vai e volta"}, {3, "Segurar no fim"}}};
inline constexpr auto animationClipCountOptions = [] {
  std::array<ComponentEnumOption, Animation::MaximumClips + 1> options{};
  constexpr const char *labels[]{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16",
                                 "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32"};
  for (u32 i = 0; i <= Animation::MaximumClips; ++i) options[i] = {i, labels[i]};
  return options;
}();
inline constexpr std::array<ComponentEnum, 2> animationEnums{{
  {"wrap_mode", "Repetição", animationWrapOptions,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Animation &>(v).wrapMode); },
   [](ComponentValue &v, u32 value) {
     static_cast<Animation &>(v).wrapMode = static_cast<resources::AnimationWrapMode>(value);
   },
   {"Reprodução", "", "Wrap Mode padrão dos clipes: o que acontece depois do fim"}},
  // Tamanho da lista `animations`: aumentar cria entradas vazias para escolher;
  // diminuir tira as últimas.
  {"clip_count", "Quantidade de clipes", animationClipCountOptions,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Animation &>(v).clips.size()); },
   [](ComponentValue &v, u32 value) { static_cast<Animation &>(v).resizeClips(value); }, {"Clipes"}}
}};
inline constexpr std::array<ComponentResourceBinding, 2> animationResources{{
  {"clip", "Clipe padrão", resources::AssetType::AnimationClip,
   [](const ComponentValue &) { return 1u; },
   [](const ComponentValue &v, u32) { return static_cast<const Animation &>(v).clip; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     static_cast<Animation &>(v).clip = value;
     return true;
   },
   {"Clipes", "", "O clipe que Play() e Tocar ao iniciar usam"}},
  {"clips", "Clipe", resources::AssetType::AnimationClip,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Animation &>(v).clips.size()); },
   [](const ComponentValue &v, u32 slot) {
     const auto &a = static_cast<const Animation &>(v);
     return slot < a.clips.size() ? a.clips[slot].asset : resources::AssetGuid{};
   },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     auto &a = static_cast<Animation &>(v);
     if (slot >= a.clips.size()) return false;
     a.clips[slot].asset = value;
     return true;
   },
   {"Clipes", "", "Clipes que este objeto pode tocar (Animation.animations)"},false,{},
   [](const ComponentValue &v,u32 slot) -> u64 {
     const auto &a=static_cast<const Animation &>(v);
     return slot<a.clips.size()?a.clips[slot].id:u64{0};
   }}
}};
inline const std::array<ComponentCollection,1> animationCollections{{
  {"clips",[](const ComponentValue &v){return static_cast<u32>(static_cast<const Animation&>(v).clips.size());},
   [](const ComponentValue &v,u32 slot){return static_cast<const Animation&>(v).clips[slot].id;},
   [](const ComponentValue &v){return static_cast<const Animation&>(v).nextClipId();},
   [](ComponentValue &v,u64 floor){return static_cast<Animation&>(v).reserveClipIdsUntil(floor);}}
}};
inline const ComponentType Animation::descriptor{
  "astra.animation", 4, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Animation>(); },
  animationNumbers, animationBooleans, animationEnums, nullptr, false, {}, {}, animationResources, {}, {}, animationCollections
};
} // namespace ae::scene
