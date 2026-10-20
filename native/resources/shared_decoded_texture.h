#pragma once
#include "core/sha256.h"
#include "renderer/authoring_texture.h"
#include <mutex>
#include <unordered_map>

namespace ae::resources {
// Revisions retain immutable decoded pixels, not another copy of each mip chain.
// Weak ownership releases unused textures when the last scene/history owner goes.
inline renderer::SharedAuthoringTexture shareDecodedTexture(renderer::AuthoringTexture made) {
  if(made.file || !made.valid())return std::make_shared<renderer::AuthoringTexture>(std::move(made));
  const auto key=Sha256::hex(made.mipChain)+":"+std::to_string(made.width)+":"+
    std::to_string(made.height)+":"+std::to_string(made.levels)+":"+
    std::to_string(made.format)+":"+std::to_string(made.srgb)+":"+std::to_string(made.samplerFlags)+":"+std::to_string(made.minimumResidentMip);
  static std::mutex mutex;
  static std::unordered_map<std::string,std::weak_ptr<const renderer::AuthoringTexture>> pool;
  const std::lock_guard lock(mutex);
  if(const auto found=pool.find(key);found!=pool.end())
    if(auto existing=found->second.lock();existing && existing->width==made.width &&
       existing->height==made.height && existing->levels==made.levels && existing->format==made.format &&
       existing->srgb==made.srgb && existing->samplerFlags==made.samplerFlags && existing->minimumResidentMip==made.minimumResidentMip &&
       existing->firstLevel==0 && !existing->file && existing->mipChain==made.mipChain)return existing;
  // Prune expired keys as well: importing different files must not grow a hidden cache.
  std::erase_if(pool,[](const auto &entry){return entry.second.expired();});
  auto texture=std::make_shared<renderer::AuthoringTexture>(std::move(made));
  pool[key]=texture;
  return texture;
}

} // namespace ae::resources
