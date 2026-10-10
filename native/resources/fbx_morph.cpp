#include "resources/fbx_morph.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "third_party/ufbx/ufbx.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <cmath>
#include <limits>
#include <set>

namespace ae::resources {
bool prepareFbxMorph(const ufbx_mesh &mesh,const FbxDeformationPolicy &policy,
    const GltfImportProgress &progress,std::vector<FbxMorphTarget> &out,std::string &diagnostic) {
  out.clear();diagnostic.clear();
  const auto fail=[&](const char *message){out.clear();diagnostic=message;return false;};
  u64 count=0;
  std::set<u32> channels;
  for(const auto *deformer:mesh.blend_deformers) {
    if(!deformer)return fail("Deformer morph FBX ausente.");
    for(const auto *channel:deformer->channels) {
      if(!channel||!channels.insert(channel->element_id).second||channel->keyframes.count==0)
        return fail("Canal morph FBX ausente, repetido ou sem alvo.");
      if(channel->keyframes.count>MaximumMorphTargets-count)
        return fail("Morph FBX excede o limite real de 64 alvos por malha.");
      double previous=-std::numeric_limits<double>::infinity();
      for(const auto &key:channel->keyframes) {
        if(!std::isfinite(key.target_weight)||key.target_weight<=previous||key.target_weight==0)
          return fail("Pesos das formas intermediárias FBX devem ser finitos, distintos e ordenados ao redor da origem.");
        previous=key.target_weight;
      }
      count+=channel->keyframes.count;
    }
  }
  if(!count)return true;
  if(mesh.num_vertices>policy.maximumBytes/(6*sizeof(float)+1))
    return fail("Morph FBX excede o orçamento de vértices de controle.");
  const u64 perTarget=sizeof(FbxMorphTarget)+static_cast<u64>(mesh.num_vertices)*6*sizeof(float);
  if(!mesh.num_vertices||mesh.num_vertices>policy.maximumBytes||perTarget>(policy.maximumBytes-mesh.num_vertices)/count)
    return fail("Morph FBX excede o orçamento de atributos por vértice de controle.");
  u64 retained=count*perTarget+mesh.num_vertices;
  out.reserve(static_cast<usize>(count));
  for(const auto *deformer:mesh.blend_deformers)for(const auto *channel:deformer->channels) {
    for(usize keyframe=0;keyframe<channel->keyframes.count;++keyframe) {
      if(progress.cancelled&&progress.cancelled(progress.context))return fail("Preparação de morph FBX cancelada.");
      const auto &key=channel->keyframes.data[keyframe];const auto *shape=key.shape;
      if(!shape||shape->offset_vertices.count!=shape->num_offsets||shape->position_offsets.count!=shape->num_offsets||
          (shape->normal_offsets.count&&shape->normal_offsets.count!=shape->num_offsets)||
          (shape->offset_weights.count&&shape->offset_weights.count!=shape->num_offsets)||
          !std::isfinite(key.effective_weight)||std::abs(key.effective_weight)>10)
        return fail("Alvo morph FBX inconsistente ou peso fora do consumidor de blend shapes.");
      FbxMorphTarget target;target.channel=channel->typed_id;target.keyframe=static_cast<u32>(keyframe);
      target.defaultWeight=static_cast<float>(key.effective_weight);
      target.name.assign(channel->name.data,channel->name.length);
      if(channel->keyframes.count>1) {target.name+=" / ";target.name.append(shape->name.data,shape->name.length);}
      if(target.name.capacity()>policy.maximumBytes-retained)return fail("Nomes morph FBX excedem o orçamento.");
      retained+=target.name.capacity();
      target.positions.resize(mesh.num_vertices*3);target.normals.resize(mesh.num_vertices*3);
      std::vector<u8> assigned(mesh.num_vertices);
      for(usize offset=0;offset<shape->num_offsets;++offset) {
        if((offset&1023)==0&&progress.cancelled&&progress.cancelled(progress.context))
          return fail("Preparação de morph FBX cancelada.");
        const auto vertex=shape->offset_vertices.data[offset];
        if(vertex>=mesh.num_vertices||assigned[vertex])
          return fail("Alvo morph FBX referencia vértice ausente ou duplicado.");
        assigned[vertex]=1;
        const double weight=shape->offset_weights.count?shape->offset_weights.data[offset]:1;
        if(!std::isfinite(weight))return fail("Peso de offset morph FBX não finito.");
        const auto position=shape->position_offsets.data[offset];
        const auto normal=shape->normal_offsets.count?shape->normal_offsets.data[offset]:ufbx_vec3{};
        for(u32 axis=0;axis<3;++axis) {
          const double p=position.v[axis]*weight,n=normal.v[axis]*weight;
          if(!std::isfinite(p)||!std::isfinite(n)||std::abs(p)>std::numeric_limits<float>::max()||
              std::abs(n)>std::numeric_limits<float>::max())return fail("Offset morph FBX não representável no consumidor float.");
          target.positions[vertex*3+axis]=static_cast<float>(p);target.normals[vertex*3+axis]=static_cast<float>(n);
        }
      }
      out.push_back(std::move(target));
    }
  }
  return true;
}
}
