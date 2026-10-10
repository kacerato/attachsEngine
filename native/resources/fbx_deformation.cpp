#include "resources/fbx_deformation.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "third_party/ufbx/ufbx.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <type_traits>

namespace ae::resources {
namespace {
bool cancelled(const GltfImportProgress &p) {return p.cancelled&&p.cancelled(p.context);}
bool nodeIndex(const ufbx_scene &scene,const ufbx_node *node,u32 &index) {
  if(!node||node->typed_id>=scene.nodes.count||scene.nodes.data[node->typed_id]!=node)return false;
  index=node->typed_id;return true;
}
}
bool prepareFbxSkin(const ufbx_mesh &mesh,const ufbx_scene &scene,const FbxDeformationPolicy &policy,
    const GltfImportProgress &progress,FbxSkinAttributes &out,std::string &diagnostic) {
  out={};diagnostic.clear();
  const auto fail=[&](const char *message){out={};diagnostic=message;return false;};
  if(mesh.skin_deformers.count!=1)return fail("FBX requer um deformer de skin por malha; combinação de deformers não integrada.");
  const auto &skin=*mesh.skin_deformers.data[0];
  if(skin.skinning_method!=UFBX_SKINNING_METHOD_LINEAR&&skin.skinning_method!=UFBX_SKINNING_METHOD_RIGID)
    return fail("FBX dual quaternion exige consumidor correspondente; este perfil aceita linear/rigid.");
  if(!skin.clusters.count||skin.clusters.count>MaximumSkinJoints)return fail("Skin FBX fora do limite de 1 a 256 juntas.");
  if(skin.vertices.count!=mesh.num_vertices||mesh.num_vertices>policy.maximumBytes/33)
    return fail("Atributos de skin FBX excedem o orçamento ou não correspondem aos vértices de controle.");
  const u64 attributeBytes=static_cast<u64>(mesh.num_vertices)*25+skin.clusters.count*68;
  if(attributeBytes>policy.maximumBytes)return fail("Skin FBX excede o orçamento de memória.");
  std::set<u32> unique;
  for(const auto *cluster:skin.clusters) {
    u32 node=0;if(!cluster||!nodeIndex(scene,cluster->bone_node,node)||!unique.insert(node).second)
      return fail("Junta FBX ausente, duplicada ou fora da hierarquia.");
    out.jointNodes.push_back(node);
    for(u32 column=0;column<4;++column)for(u32 row=0;row<4;++row) {
      const double value=row==3?(column==3?1:0):cluster->geometry_to_bone.cols[column].v[row];
      if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail("Matriz de bind FBX não finita.");
      out.inverseBinds.push_back(static_cast<float>(value));
    }
  }
  std::vector<u8> used(mesh.num_vertices);
  for(const auto vertex:mesh.vertex_indices) {
    if(vertex>=mesh.num_vertices)return fail("Índice FBX fora dos vértices de controle.");
    used[vertex]=1;
  }
  out.joints.resize(mesh.num_vertices*4);out.weights.resize(mesh.num_vertices*4);
  for(usize vertex=0;vertex<mesh.num_vertices;++vertex) {
    if(cancelled(progress))return fail("Preparação de skin FBX cancelada.");
    if(!used[vertex])continue;
    const auto &binding=skin.vertices.data[vertex];
    if(binding.weight_begin>skin.weights.count||binding.num_weights>skin.weights.count-binding.weight_begin)
      return fail("Faixa de influências FBX fora do deformer.");
    double sum=0;double strongest[4]{};u32 nonzero=0;std::set<u32> assigned;
    for(u32 n=0;n<binding.num_weights;++n) {
      const auto &weight=skin.weights.data[binding.weight_begin+n];
      if(weight.cluster_index>=skin.clusters.count||!std::isfinite(weight.weight)||weight.weight<0||
          !assigned.insert(weight.cluster_index).second)return fail("Influência FBX inválida ou junta duplicada no vértice.");
      if(weight.weight==0)continue;
      if(nonzero<4) {out.joints[vertex*4+nonzero]=static_cast<u16>(weight.cluster_index);strongest[nonzero]=weight.weight;sum+=weight.weight;}
      ++nonzero;
    }
    if(nonzero>4) {
      if(!policy.reduceWeightsToFour)return fail("Skin FBX possui mais de quatro influências. Autorize redução explícita na receita.");
      out.reducedWeights=true;
    }
    if(!std::isfinite(sum)||sum<=0)return fail("Vértice FBX usado sem peso positivo; uma junta arbitrária não será inventada.");
    for(u32 n=0;n<std::min(nonzero,4u);++n)out.weights[vertex*4+n]=static_cast<float>(strongest[n]/sum);
  }
  return true;
}

bool prepareFbxTakes(const ufbx_scene &scene,const FbxDeformationPolicy &policy,
    const GltfImportProgress &progress,std::vector<FbxBakedTake> &out,std::string &diagnostic) {
  out.clear();diagnostic.clear();
  const auto fail=[&](const char *message){out.clear();diagnostic=message;return false;};
  if((policy.bakeRate!=30&&policy.bakeRate!=60)||!policy.maximumBytes||!policy.maximumKeys||
      scene.anim_stacks.count>policy.maximumTakes)return fail("Política de bake FBX inválida ou takes acima do orçamento.");
  u64 keys=0,bytes=0;
  u64 sourceKeys=0;bool sourceSteps=false;
  const u64 sourceKeyBudget=std::min<u64>(policy.maximumBytes/sizeof(ufbx_keyframe),
      policy.maximumKeys>std::numeric_limits<u64>::max()/16?std::numeric_limits<u64>::max():policy.maximumKeys*16);
  for(const auto *curve:scene.anim_curves) {
    if(!curve||curve->keyframes.count>sourceKeyBudget-sourceKeys)return fail("Curvas FBX excedem o orçamento de chaves antes do bake.");
    sourceKeys+=curve->keyframes.count;
    for(usize index=0;index+1<curve->keyframes.count;++index) {
      const auto &key=curve->keyframes.data[index];
      if(key.value!=curve->keyframes.data[index+1].value&&
          (key.interpolation==UFBX_INTERPOLATION_CONSTANT_PREV||key.interpolation==UFBX_INTERPOLATION_CONSTANT_NEXT))sourceSteps=true;
    }
  }
  // ufbx's maximum-rate pass replaces step flags with reduced sample flags.
  // Check the actual source discontinuities before that pass can erase them.
  if(sourceSteps&&!policy.allowStepApproximation)return fail("FBX contém steps; autorize sua aproximação pela taxa de bake.");
  for(const auto *stack:scene.anim_stacks) {
    if(cancelled(progress))return fail("Preparação de takes FBX cancelada.");
    if(!stack||!stack->anim)return fail("Take FBX sem descritor de animação.");
    const double duration=stack->time_end-stack->time_begin;
    if(!std::isfinite(duration)||duration<0||duration>65536)return fail("Duração do take FBX fora do perfil suportado.");
    // Bounds the coarse sampling work before asking ufbx to allocate a bake.
    if(duration*policy.bakeRate>static_cast<double>(policy.maximumKeys))return fail("Take FBX excede o orçamento de amostragem.");
    ufbx_bake_opts options{};options.temp_allocator.memory_limit=policy.maximumBytes;
    options.result_allocator.memory_limit=policy.maximumBytes;
    options.resample_rate=policy.bakeRate;options.minimum_sample_rate=policy.bakeRate;
    options.maximum_sample_rate=policy.bakeRate;
    options.max_keyframe_segments=32;options.step_handling=UFBX_BAKE_STEP_HANDLING_DEFAULT;
    ufbx_error error{};
    std::unique_ptr<ufbx_baked_anim,decltype(&ufbx_free_baked_anim)> baked(ufbx_bake_anim(&scene,stack->anim,&options,&error),ufbx_free_baked_anim);
    if(!baked)return fail("Bake nativo FBX falhou ou excedeu o orçamento de memória.");
    if(cancelled(progress))return fail("Preparação de takes FBX cancelada.");
    for(const auto &element:baked->elements)for(const auto &property:element.props)
      if(!property.constant_value) {
        const auto *source=element.element_id<scene.elements.count?scene.elements.data[element.element_id]:nullptr;
        const bool morph=policy.sampleMorph&&source&&source->type==UFBX_ELEMENT_BLEND_CHANNEL&&
            std::string_view(property.name.data,property.name.length)=="DeformPercent";
        if(!morph)return fail("Take FBX anima propriedade sem consumidor TRS/morph. Conversão parcial recusada.");
      }
    FbxBakedTake take;take.name={stack->name.data,stack->name.length};take.approximatedSteps=sourceSteps;
    // FBX often retains keys outside a named take. Honor its declared interval,
    // sampling both boundaries; files without an interval use actual key bounds.
    take.sourceStart=baked->playback_duration>0?baked->playback_time_begin:baked->key_time_min;
    const double end=baked->playback_duration>0?baked->playback_time_end:baked->key_time_max;
    if(!std::isfinite(take.sourceStart)||!std::isfinite(end)||end<take.sourceStart)
      return fail("Intervalo efetivo do take FBX inválido.");
    take.duration=static_cast<float>(end-take.sourceStart);
    if(!std::isfinite(take.duration)||take.duration<0||take.duration>65536)return fail("Intervalo assado FBX inválido.");
    for(const auto &node:baked->nodes) {
      if(cancelled(progress))return fail("Preparação de takes FBX cancelada.");
      if(node.typed_id>=scene.nodes.count)return fail("Canal FBX referencia nó ausente.");
      const auto append=[&](const auto &source,AnimationPath path)->bool {
        if(source.count==0)return true;
        const auto reject=[&](const char *message){diagnostic=message;return false;};
        using Key=std::remove_cvref_t<decltype(source.data[0])>;
        if(policy.maximumBytes/sizeof(Key)<2||source.count>policy.maximumBytes/sizeof(Key)-2)
          return reject("Bake FBX excede orçamento temporário de chaves.");
        std::vector<Key> selected;selected.reserve(source.count+2);
        const auto boundary=[&](double time) {
          Key key{};key.time=time;
          if constexpr(requires{key.value.w;})key.value=ufbx_evaluate_baked_quat(source,time);
          else key.value=ufbx_evaluate_baked_vec3(source,time);
          selected.push_back(key);
        };
        boundary(take.sourceStart);
        for(const auto &key:source)if(key.time>take.sourceStart&&key.time<end)selected.push_back(key);
        if(end>take.sourceStart)boundary(end);
        const u32 components=path==AnimationPath::Rotation?4:3;
        const u64 required=selected.size()*(components+1ull)*sizeof(float)+sizeof(AnimationChannel);
        if(selected.size()>policy.maximumKeys-keys||required>policy.maximumBytes-bytes)
          return reject("Take FBX excede o orçamento agregado de canais/chaves do bake.");
        keys+=selected.size();bytes+=required;
        AnimationChannel channel;channel.node=node.typed_id;channel.path=path;
        channel.times.reserve(selected.size());channel.values.reserve(selected.size()*components);
        float previous=-1;
        for(const auto &key:selected) {
          const float time=static_cast<float>(key.time-take.sourceStart);
          if(!std::isfinite(time)||time<0)return reject("Take FBX contém tempo negativo ou não finito após trim.");
          if(time<=previous)return reject("Take FBX contém tempos que colapsam na precisão float do consumidor.");
          if(time>take.duration+.001f)return reject("Take FBX contém chave além do intervalo de reprodução declarado.");
          previous=time;channel.times.push_back(time);
          if(key.flags&(UFBX_BAKED_KEY_STEP_LEFT|UFBX_BAKED_KEY_STEP_RIGHT|UFBX_BAKED_KEY_STEP_KEY)) {
            if(!policy.allowStepApproximation)return reject("Take FBX requer autorização para aproximar steps por transição curta.");
            take.approximatedSteps=true;
          }
          const double values[]{key.value.x,key.value.y,key.value.z};
          for(const auto value:values) {
            if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())
              return reject("Take FBX contém valor TRS não representável pelo consumidor float.");
            channel.values.push_back(static_cast<float>(value));
          }
          if constexpr(requires{key.value.w;}) {
            if(!std::isfinite(key.value.w))return reject("Take FBX contém rotação não finita.");
            const double length=std::hypot(std::hypot(key.value.x,key.value.y),std::hypot(key.value.z,key.value.w));
            if(!std::isfinite(length)||length<1e-12)return reject("Take FBX contém quaternion degenerado.");
            for(usize c=0;c<3;++c)channel.values[channel.values.size()-3+c]=static_cast<float>(values[c]/length);
            channel.values.push_back(static_cast<float>(key.value.w/length));
          }
        }
        take.channels.push_back(std::move(channel));return true;
      };
      if(!append(node.translation_keys,AnimationPath::Translation)||!append(node.rotation_keys,AnimationPath::Rotation)||
          !append(node.scale_keys,AnimationPath::Scale))
        {out.clear();return false;}
    }
    if(take.channels.empty()&&!policy.sampleMorph)return fail("Take FBX sem canais TRS efetivos; não será publicado como clipe vazio.");
    out.push_back(std::move(take));
  }
  return true;
}
}
