#include "resources/fbx_scene_import.h"
#include "resources/fbx_morph.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "third_party/ufbx/ufbx.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <locale>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace ae::resources {
namespace {
std::string stringOf(ufbx_string value) {return {value.data,value.length};}
std::string jsonString(std::string_view value) {
  constexpr char digits[]="0123456789abcdef";std::string out="\"";
  for(const unsigned char c:value) {
    if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
    else if(c<32){out+="\\u00";out+=digits[c>>4];out+=digits[c&15];}
    else out+=static_cast<char>(c);
  }
  return out+'"';
}
void word(std::vector<u8> &out,u32 value) {for(u32 i=0;i<4;++i)out.push_back(static_cast<u8>(value>>(i*8)));}
std::ostringstream jsonStream() {std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);return out;}
template<class T>void join(std::ostream &out,const T &items) {bool first=true;for(const auto &item:items){if(!first)out<<',';out<<item;first=false;}}
struct GlbData {
  std::vector<u8> binary;
  std::vector<std::string> views,accessors;
  usize maximum=0;
  bool floats(std::span<const float> values,u32 components,const char *type,u32 &accessor) {
    if(values.empty()||values.size()%components||values.size()>maximum/4||binary.size()>maximum-values.size()*4)return false;
    const auto begin=binary.size();for(const auto value:values) {if(!std::isfinite(value))return false;word(binary,std::bit_cast<u32>(value));}
    auto view=jsonStream();view<<"{\"buffer\":0,\"byteOffset\":"<<begin<<",\"byteLength\":"<<values.size()*4<<'}';views.push_back(view.str());
    auto item=jsonStream();item<<"{\"bufferView\":"<<views.size()-1<<",\"componentType\":5126,\"count\":"<<values.size()/components<<",\"type\":\""<<type<<"\"}";
    accessor=static_cast<u32>(accessors.size());accessors.push_back(item.str());return true;
  }
  bool shorts(std::span<const u16> values,u32 &accessor) {
    if(values.empty()||values.size()%4||values.size()>maximum/2||binary.size()>maximum-values.size()*2)return false;
    while(binary.size()%4)binary.push_back(0);
    if(binary.size()>maximum-values.size()*2)return false;
    const auto begin=binary.size();
    for(const auto value:values){binary.push_back(static_cast<u8>(value));binary.push_back(static_cast<u8>(value>>8));}
    auto view=jsonStream();view<<"{\"buffer\":0,\"byteOffset\":"<<begin<<",\"byteLength\":"<<values.size()*2<<'}';views.push_back(view.str());
    auto item=jsonStream();item<<"{\"bufferView\":"<<views.size()-1<<",\"componentType\":5123,\"count\":"<<values.size()/4<<",\"type\":\"VEC4\"}";
    accessor=static_cast<u32>(accessors.size());accessors.push_back(item.str());return true;
  }
};
struct Progress {const GltfImportProgress *source=nullptr;};
// Same zero-anchored piecewise weights used by ufbx0.20.0's blend-channel
// evaluation. Each independent glTF target is a single original key shape.
double shapeWeight(const ufbx_blend_channel &channel,usize keyframe,double value) {
  i64 negative=-1,previous=-1,next=-1;
  for(usize i=0;i<channel.keyframes.count;++i)if(channel.keyframes.data[i].target_weight<0)negative=static_cast<i64>(i);
  if(value>0) {
    for(usize i=static_cast<usize>(negative+1);i<channel.keyframes.count;++i) {
      previous=next;next=static_cast<i64>(i);if(channel.keyframes.data[i].target_weight>value)break;
    }
  } else {
    for(i64 i=negative;i>=0;--i) {
      previous=next;next=i;if(channel.keyframes.data[static_cast<usize>(i)].target_weight<value)break;
    }
  }
  const double a=previous>=0?channel.keyframes.data[static_cast<usize>(previous)].target_weight:0;
  const double b=next>=0?channel.keyframes.data[static_cast<usize>(next)].target_weight:0;
  if(a==b)return 0;
  const double t=(value-a)/(b-a);
  return static_cast<i64>(keyframe)==previous?1-t:static_cast<i64>(keyframe)==next?t:0;
}
ufbx_progress_result loading(void *context,const ufbx_progress *value) {
  const auto &p=*static_cast<Progress*>(context)->source;
  if(p.cancelled&&p.cancelled(p.context))return UFBX_PROGRESS_CANCEL;
  if(p.report)p.report(p.context,value->bytes_total?static_cast<float>(value->bytes_read)/value->bytes_total*.35f:0,"Lendo FBX nativo");
  return UFBX_PROGRESS_CONTINUE;
}
}
bool importFbxScene(std::span<const u8> bytes,const GltfImportLimits &limits,
                     const GltfImportProgress &progress,const FbxSceneImportPolicy &policy,GltfImport &out) {
  out={};const auto fail=[&](std::string message){out={};out.cancelled=progress.cancelled&&progress.cancelled(progress.context);out.diagnostic=std::move(message);return false;};
  const auto cancelled=[&](){return progress.cancelled&&progress.cancelled(progress.context);};
  if(bytes.empty()||bytes.size()>limits.maximumBytes)return fail("FBX vazio ou maior que o limite da importação.");
  if(!std::isfinite(limits.rootScale)||limits.rootScale<=0)return fail("Escala de raiz FBX inválida.");
  if(cancelled()){out.cancelled=true;out.diagnostic="Importação FBX cancelada.";return false;}
  const usize budget=static_cast<usize>(std::min<u64>(limits.maximumExpandedBytes/4,128u<<20));
  if(!budget)return fail("Orçamento de memória FBX indisponível.");
  ufbx_load_opts options{};options.temp_allocator.memory_limit=budget;options.result_allocator.memory_limit=budget;
  options.file_format=UFBX_FILE_FORMAT_FBX;
  options.load_external_files=false;options.strict=true;options.node_depth_limit=128;
  options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;
  options.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  options.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
  options.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_COMPENSATE;
  options.pivot_handling=UFBX_PIVOT_HANDLING_ADJUST_TO_PIVOT;
  options.generate_missing_normals=true;options.use_blender_pbr_material=true;
  Progress context{&progress};options.progress_cb.fn=loading;options.progress_cb.user=&context;options.progress_interval_hint=65536;
  ufbx_error error{};std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(ufbx_load_memory(bytes.data(),bytes.size(),&options,&error),ufbx_free_scene);
  if(!scene) {
    if(cancelled()||error.type==UFBX_ERROR_CANCELLED){out.cancelled=true;out.diagnostic="Importação FBX cancelada.";return false;}
    char description[512]{};ufbx_format_error(description,sizeof(description),&error);return fail("FBX recusado: "+std::string(description));
  }
  if(scene->nodes.count>limits.maximumNodes)return fail("FBX excede o limite de nós.");
  if(scene->materials.count>limits.maximumDraws)return fail("FBX excede o limite de materiais deste perfil.");
  if(!policy.deformation&&(scene->skin_deformers.count||scene->blend_deformers.count||scene->anim_stacks.count||scene->constraints.count))
    return fail("Este perfil FBX estático não converte skin, morph, takes ou constraints. Fonte preservada; use o perfil com essas capacidades quando disponível.");
  if(scene->constraints.count)return fail("Constraints FBX ainda não possuem conversão fiel neste perfil.");
  if(scene->nurbs_curves.count||scene->nurbs_surfaces.count)return fail("FBX contém NURBS; tesselação desse recurso ainda não integrada.");
  if((limits.importCameras&&scene->cameras.count)||(limits.importLights&&scene->lights.count))
    return fail("Câmeras/luzes FBX ainda não são convertidas neste perfil. Desligue sua importação explicitamente para continuar preservando a fonte.");
  std::map<const ufbx_node*,u32> nodeIds;for(usize i=0;i<scene->nodes.count;++i)nodeIds.emplace(scene->nodes.data[i],static_cast<u32>(i));
  std::vector<std::string> nodes,meshes,materials,skins,animations;std::set<u32> omittedTextures;
  std::vector<FbxBakedTake> takes;
  std::string deformationDiagnostic;
  auto bake=policy.bake;bake.sampleMorph=policy.deformation;bake.maximumBytes=std::min<u64>(bake.maximumBytes,budget);
  bake.maximumKeys=std::min<u64>(bake.maximumKeys,budget/32);
  if(policy.deformation&&scene->anim_stacks.count&&!prepareFbxTakes(*scene,bake,progress,takes,deformationDiagnostic))
    return fail(deformationDiagnostic);
  struct MorphBinding {u32 node=0;std::vector<std::pair<u32,u32>> targets;};
  std::vector<MorphBinding> morphBindings;
  bool reducedWeights=false,recalculateTangents=false,droppedTangents=false,linearFromDualQuaternion=false;
  for(const auto *material:scene->materials) {
    const auto &p=material->pbr;const auto &f=material->fbx;
    const auto color=p.base_color.has_value?p.base_color.value_vec3:(f.diffuse_color.has_value?f.diffuse_color.value_vec3:ufbx_vec3{{{1,1,1}}});
    const double factor=p.base_factor.has_value?p.base_factor.value_real:(f.diffuse_factor.has_value?f.diffuse_factor.value_real:1);
    const double alpha=p.opacity.has_value?p.opacity.value_real:1;
    const double roughness=p.roughness.has_value?p.roughness.value_real:1;
    const double metallic=p.metalness.has_value?p.metalness.value_real:0;
    const auto emission=p.emission_color.has_value?p.emission_color.value_vec3:f.emission_color.value_vec3;
    const double strength=p.emission_factor.has_value?p.emission_factor.value_real:(f.emission_factor.has_value?f.emission_factor.value_real:0);
    const double values[]{color.x*factor,color.y*factor,color.z*factor,alpha,roughness,metallic,emission.x*strength,emission.y*strength,emission.z*strength};
    for(const auto value:values)if(!std::isfinite(value)||value<0||value>1)return fail("Fatores de material FBX fora do perfil PBR representável.");
    auto item=jsonStream();item<<"{\"name\":"<<jsonString(stringOf(material->name))<<",\"pbrMetallicRoughness\":{\"baseColorFactor\":["<<values[0]<<','<<values[1]<<','<<values[2]<<','<<alpha<<"],\"roughnessFactor\":"<<roughness<<",\"metallicFactor\":"<<metallic<<"},\"emissiveFactor\":["<<values[6]<<','<<values[7]<<','<<values[8]<<']';
    if(alpha<1)item<<",\"alphaMode\":\"BLEND\"";
    if(material->features.double_sided.enabled)item<<",\"doubleSided\":true";
    item<<'}';materials.push_back(item.str());
    for(const auto &map:p.maps)if(map.texture&&map.texture_enabled)omittedTextures.insert(map.texture->element_id);
    for(const auto &map:f.maps)if(map.texture&&map.texture_enabled)omittedTextures.insert(map.texture->element_id);
  }
  GlbData data;data.maximum=budget;u64 vertexCount=0;u32 primitiveCount=0;
  for(usize nodeIndex=0;nodeIndex<scene->nodes.count;++nodeIndex) {
    if(cancelled()){out={};out.cancelled=true;out.diagnostic="Importação FBX cancelada.";return false;}
    const auto *node=scene->nodes.data[nodeIndex];auto item=jsonStream();item<<"{\"name\":"<<jsonString(stringOf(node->name));
    const auto &matrix=node->node_to_parent;
    if(policy.deformation) {
      const auto &transform=node->local_transform;const auto rebuilt=ufbx_transform_to_matrix(&transform);
      for(u32 c=0;c<4;++c)for(u32 r=0;r<3;++r) {
        const double value=matrix.cols[c].v[r],candidate=rebuilt.cols[c].v[r];
        if(!std::isfinite(value)||!std::isfinite(candidate)||std::abs(value-candidate)>1e-5*std::max(1.0,std::abs(value)))
          return fail("Nó FBX animável contém shear ou transformação incompatível com TRS.");
      }
      const auto &q=transform.rotation;const double length=std::hypot(std::hypot(q.x,q.y),std::hypot(q.z,q.w));
      if(!std::isfinite(length)||length<1e-12)return fail("Rotação local FBX degenerada.");
      const double values[]{transform.translation.x,transform.translation.y,transform.translation.z,
          q.x/length,q.y/length,q.z/length,q.w/length,transform.scale.x,transform.scale.y,transform.scale.z};
      for(const auto value:values)if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())
        return fail("Transformação FBX não representável em float.");
      item<<",\"translation\":["<<values[0]<<','<<values[1]<<','<<values[2]<<"],\"rotation\":["<<values[3]<<','<<values[4]<<','<<values[5]<<','<<values[6]
          <<"],\"scale\":["<<values[7]<<','<<values[8]<<','<<values[9]<<']';
    } else {
    item<<",\"matrix\":[";
    for(u32 column=0;column<4;++column)for(u32 row=0;row<4;++row) {
      if(column||row)item<<',';
      const double value=row==3?(column==3?1:0):matrix.cols[column].v[row];
      if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail("Transformação FBX não finita.");
      item<<value;
    }
    item<<']';
    }
    if(node->children.count) {item<<",\"children\":[";for(usize c=0;c<node->children.count;++c){if(c)item<<',';item<<nodeIds.at(node->children.data[c]);}item<<']';}
    if(node->mesh) {
      const auto *mesh=node->mesh;std::map<u32,std::vector<ufbx_face>> groups;
      if(mesh->num_indices>limits.maximumIndices)return fail("Índices FBX excedem o orçamento do preparador.");
      bool sourceTangents=mesh->vertex_normal.exists&&mesh->vertex_tangent.exists&&mesh->vertex_bitangent.exists&&
          limits.tangents==GltfTangentsImport&&limits.normals==GltfNormalsImport;
      if(sourceTangents)for(u32 index=0;index<mesh->num_indices;++index) {
        const auto n=ufbx_get_vertex_vec3(&mesh->vertex_normal,index),t=ufbx_get_vertex_vec3(&mesh->vertex_tangent,index),
            b=ufbx_get_vertex_vec3(&mesh->vertex_bitangent,index);
        const double orientation=(n.y*t.z-n.z*t.y)*b.x+(n.z*t.x-n.x*t.z)*b.y+(n.x*t.y-n.y*t.x)*b.z;
        if(!std::isfinite(orientation)||std::abs(orientation)<1e-12) {
          sourceTangents=false;droppedTangents=true;recalculateTangents=recalculateTangents||mesh->vertex_uv.exists;break;
        }
      }
      FbxSkinAttributes skin;std::vector<FbxMorphTarget> morph;
      if(policy.deformation&&mesh->skin_deformers.count) {
        if(!prepareFbxSkin(*mesh,*scene,bake,progress,skin,deformationDiagnostic))return fail(deformationDiagnostic);
        reducedWeights=reducedWeights||skin.reducedWeights;linearFromDualQuaternion=linearFromDualQuaternion||skin.linearFromDualQuaternion;
        u32 binds=0;if(!data.floats(skin.inverseBinds,16,"MAT4",binds))return fail("Binds FBX excedem o orçamento.");
        auto encoded=jsonStream();encoded<<"{\"name\":"<<jsonString(stringOf(mesh->name))<<",\"inverseBindMatrices\":"<<binds<<",\"joints\":[";
        join(encoded,skin.jointNodes);encoded<<"]}";item<<",\"skin\":"<<skins.size();skins.push_back(encoded.str());
      }
      if(policy.deformation&&mesh->blend_deformers.count&&
          !prepareFbxMorph(*mesh,bake,progress,morph,deformationDiagnostic))return fail(deformationDiagnostic);
      if(!morph.empty()) {
        MorphBinding binding;binding.node=static_cast<u32>(nodeIndex);
        for(const auto &target:morph)binding.targets.emplace_back(target.channel,target.keyframe);
        morphBindings.push_back(std::move(binding));
      }
      if(!mesh->vertex_position.exists||!mesh->vertex_normal.exists)return fail("FBX sem posição ou normal válida após preparação.");
      for(usize f=0;f<mesh->faces.count;++f) {
        const auto face=mesh->faces.data[f];if(face.num_indices<3)return fail("FBX contém faces de ponto/linha fora do perfil triangular.");
        const u32 material=f<mesh->face_material.count?mesh->face_material.data[f]:UFBX_NO_INDEX;
        groups[material].push_back(face);
      }
      auto built=jsonStream();built<<"{\"name\":"<<jsonString(stringOf(mesh->name))<<",\"primitives\":[";bool first=true;
      for(const auto &[material,faces]:groups) {
        if(++primitiveCount>limits.maximumDraws)return fail("FBX excede o limite de desenhos.");
        std::vector<float> positions,normals,uv,uv1,colors,tangents,weights;
        std::vector<u16> joints;
        std::vector<std::vector<float>> morphPositions(morph.size()),morphNormals(morph.size());
        const bool hasTangents=sourceTangents;
        for(const auto face:faces) {
          if(cancelled()){out={};out.cancelled=true;out.diagnostic="Importação FBX cancelada.";return false;}
          const u64 maximum=static_cast<u64>(face.num_indices-2)*3;
          if(maximum>limits.maximumIndices||maximum>limits.maximumVertices||vertexCount>limits.maximumVertices-maximum||vertexCount>limits.maximumIndices-maximum||maximum>budget/sizeof(u32))return fail("FBX excede o orçamento de triangulação.");
          const u64 components=6+(hasTangents?4:0)+(mesh->vertex_uv.exists?2:0)+(mesh->uv_sets.count>1&&mesh->uv_sets.data[1].vertex_uv.exists?2:0)+(mesh->vertex_color.exists?4:0)+
              (skin.joints.empty()?0:6)+morph.size()*6;
          u64 existing=positions.size()+normals.size()+uv.size()+uv1.size()+colors.size()+tangents.size()+weights.size()+joints.size()/2;
          for(usize target=0;target<morph.size();++target)existing+=morphPositions[target].size()+morphNormals[target].size();
          if(maximum>budget/(components*sizeof(float))||existing>budget/sizeof(float)-maximum*components)return fail("Atributos FBX excedem o orçamento temporário.");
          std::vector<u32> triangles(static_cast<usize>(maximum));const auto count=ufbx_triangulate_face(triangles.data(),triangles.size(),mesh,face)*3;
          if(!count||count>triangles.size())return fail("Triangulação FBX falhou.");
          vertexCount+=count;
          for(u32 t=0;t<count;++t) {
            const u32 index=triangles[t];const auto p=ufbx_get_vertex_vec3(&mesh->vertex_position,index);const auto n=ufbx_get_vertex_vec3(&mesh->vertex_normal,index);
            positions.insert(positions.end(),{static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)});
            normals.insert(normals.end(),{static_cast<float>(n.x),static_cast<float>(n.y),static_cast<float>(n.z)});
            const auto vertex=mesh->vertex_indices.data[index];
            if(!skin.joints.empty()) {
              joints.insert(joints.end(),skin.joints.begin()+vertex*4,skin.joints.begin()+vertex*4+4);
              weights.insert(weights.end(),skin.weights.begin()+vertex*4,skin.weights.begin()+vertex*4+4);
            }
            for(usize target=0;target<morph.size();++target) {
              const auto &attribute=morph[target];
              morphPositions[target].insert(morphPositions[target].end(),attribute.positions.begin()+vertex*3,attribute.positions.begin()+vertex*3+3);
              morphNormals[target].insert(morphNormals[target].end(),attribute.normals.begin()+vertex*3,attribute.normals.begin()+vertex*3+3);
            }
            if(hasTangents) {
              const auto tangent=ufbx_get_vertex_vec3(&mesh->vertex_tangent,index),bitangent=ufbx_get_vertex_vec3(&mesh->vertex_bitangent,index);
              const auto orientation=(n.y*tangent.z-n.z*tangent.y)*bitangent.x+(n.z*tangent.x-n.x*tangent.z)*bitangent.y+(n.x*tangent.y-n.y*tangent.x)*bitangent.z;
              if(!std::isfinite(orientation)||std::abs(orientation)<1e-12)return fail("Base tangente FBX degenerada; recalcule tangentes na fonte.");
              // The V-axis flip reverses bitangent handedness.
              tangents.insert(tangents.end(),{static_cast<float>(tangent.x),static_cast<float>(tangent.y),static_cast<float>(tangent.z),orientation<0?1.f:-1.f});
            }
            if(mesh->vertex_uv.exists) {const auto value=ufbx_get_vertex_vec2(&mesh->vertex_uv,index);uv.insert(uv.end(),{static_cast<float>(value.x),static_cast<float>(1-value.y)});}
            if(mesh->uv_sets.count>1&&mesh->uv_sets.data[1].vertex_uv.exists) {const auto value=ufbx_get_vertex_vec2(&mesh->uv_sets.data[1].vertex_uv,index);uv1.insert(uv1.end(),{static_cast<float>(value.x),static_cast<float>(1-value.y)});}
            if(mesh->vertex_color.exists) {const auto value=ufbx_get_vertex_vec4(&mesh->vertex_color,index);colors.insert(colors.end(),{static_cast<float>(value.x),static_cast<float>(value.y),static_cast<float>(value.z),static_cast<float>(value.w)});}
          }
          if((positions.size()+normals.size()+uv.size()+uv1.size()+colors.size()+tangents.size())>budget/sizeof(float))return fail("Atributos FBX excedem o orçamento temporário.");
        }
        u32 position=0,normal=0,texcoord=0,second=0,color=0,tangent=0;
        if(!data.floats(positions,3,"VEC3",position)||!data.floats(normals,3,"VEC3",normal)||
           (!uv.empty()&&!data.floats(uv,2,"VEC2",texcoord))||(!uv1.empty()&&!data.floats(uv1,2,"VEC2",second))||
           (!colors.empty()&&!data.floats(colors,4,"VEC4",color))||(!tangents.empty()&&!data.floats(tangents,4,"VEC4",tangent)))return fail("Atributos FBX não finitos ou maiores que o orçamento.");
        if(!first)built<<',';
        first=false;built<<"{\"attributes\":{\"POSITION\":"<<position<<",\"NORMAL\":"<<normal;
        if(!uv.empty())built<<",\"TEXCOORD_0\":"<<texcoord;
        if(!uv1.empty())built<<",\"TEXCOORD_1\":"<<second;
        if(!colors.empty())built<<",\"COLOR_0\":"<<color;
        if(!tangents.empty())built<<",\"TANGENT\":"<<tangent;
        if(!joints.empty()) {
          u32 jointAccessor=0,weightAccessor=0;
          if(!data.shorts(joints,jointAccessor)||!data.floats(weights,4,"VEC4",weightAccessor))return fail("Influências FBX excedem o orçamento.");
          built<<",\"JOINTS_0\":"<<jointAccessor<<",\"WEIGHTS_0\":"<<weightAccessor;
        }
        built<<'}';
        if(!morph.empty()) {
          built<<",\"targets\":[";
          for(usize target=0;target<morph.size();++target) {
            u32 p=0,n=0;
            if(!data.floats(morphPositions[target],3,"VEC3",p)||!data.floats(morphNormals[target],3,"VEC3",n))
              return fail("Alvos morph FBX excedem o orçamento.");
            if(target)built<<',';
            built<<"{\"POSITION\":"<<p<<",\"NORMAL\":"<<n<<'}';
          }
          built<<']';
        }
        if(material!=UFBX_NO_INDEX) {
          if(material>=node->materials.count)return fail("Slot de material FBX não resolvido.");
          built<<",\"material\":"<<node->materials.data[material]->typed_id;
        }
        built<<'}';
      }
      built<<']';
      if(!morph.empty()) {
        built<<",\"weights\":[";
        for(usize target=0;target<morph.size();++target){if(target)built<<',';built<<morph[target].defaultWeight;}
        built<<"],\"extras\":{\"targetNames\":[";
        for(usize target=0;target<morph.size();++target){if(target)built<<',';built<<jsonString(morph[target].name);}
        built<<"]}";
      }
      built<<'}';item<<",\"mesh\":"<<meshes.size();meshes.push_back(built.str());
    }
    item<<'}';nodes.push_back(item.str());
    if(progress.report)progress.report(progress.context,.35f+.25f*static_cast<float>(nodeIndex+1)/scene->nodes.count,"Convertendo geometria FBX");
  }
  u64 animationKeys=0,animationBytes=0;
  for(const auto &take:takes)for(const auto &channel:take.channels) {
    animationKeys+=channel.times.size();animationBytes+=(channel.times.size()+channel.values.size())*sizeof(float);
  }
  for(usize takeIndex=0;takeIndex<takes.size();++takeIndex) {
    auto &take=takes[takeIndex];const auto *stack=scene->anim_stacks.data[takeIndex];
    for(const auto &binding:morphBindings) {
      bool animated=false,stepped=false;
      for(const auto &[channelId,keyframe]:binding.targets) {
        (void)keyframe;const auto *channel=scene->blend_channels.data[channelId];
        for(const auto *layer:stack->layers)if(const auto *property=ufbx_find_anim_prop(layer,&channel->element,"DeformPercent")) {
          if(!property->anim_value)continue;
          for(const auto *curve:property->anim_value->curves)if(curve&&curve->keyframes.count!=0) {
            animated=true;
            for(const auto &key:curve->keyframes)stepped=stepped||key.interpolation==UFBX_INTERPOLATION_CONSTANT_PREV||
                key.interpolation==UFBX_INTERPOLATION_CONSTANT_NEXT;
          }
        }
      }
      if(!animated)continue;
      if(stepped&&!bake.allowStepApproximation)return fail("Morph FBX contém steps; autorize a aproximação pela taxa de bake.");
      take.approximatedSteps=take.approximatedSteps||stepped;
      const auto count=static_cast<u64>(std::ceil(static_cast<double>(take.duration)*bake.bakeRate))+1;
      const auto required=count*(binding.targets.size()+1)*sizeof(float);
      if(count>bake.maximumKeys-animationKeys||required>bake.maximumBytes-animationBytes)
        return fail("Amostragem morph FBX excede o orçamento agregado de animação.");
      animationKeys+=count;animationBytes+=required;
      AnimationChannel channel;channel.node=binding.node;channel.path=AnimationPath::Weights;
      channel.weightCount=static_cast<u32>(binding.targets.size());
      channel.times.reserve(static_cast<usize>(count));channel.values.reserve(static_cast<usize>(count)*channel.weightCount);
      for(u64 frame=0;frame<count;++frame) {
        if(cancelled()){out.cancelled=true;out.diagnostic="Amostragem morph FBX cancelada.";return false;}
        const double local=frame+1==count?take.duration:static_cast<double>(frame)/bake.bakeRate;
        const float time=static_cast<float>(local);
        if(!channel.times.empty()&&time<=channel.times.back())return fail("Tempos morph FBX colapsam no consumidor float.");
        channel.times.push_back(time);
        for(const auto &[channelId,keyframe]:binding.targets) {
          const auto &source=*scene->blend_channels.data[channelId];
          const double weight=shapeWeight(source,keyframe,ufbx_evaluate_blend_weight(stack->anim,&source,take.sourceStart+local));
          if(!std::isfinite(weight)||std::abs(weight)>10)return fail("Peso morph FBX excede o consumidor de blend shapes.");
          channel.values.push_back(static_cast<float>(weight));
        }
      }
      take.channels.push_back(std::move(channel));
    }
  }
  u32 emptyTakes=0;
  std::erase_if(takes,[&](const auto &take){if(take.channels.empty()){++emptyTakes;return true;}return false;});
  if(!primitiveCount) {
    if(!policy.deformation||takes.empty())return fail("FBX sem geometria nem canais de animação utilizáveis.");
    std::vector<u32> originalOrder;
    const auto emit=[&](auto &&self,const ufbx_node *node,i32 parent)->void {
      const auto index=static_cast<u32>(out.nodes.size());originalOrder.push_back(node->typed_id);
      GltfImportNode imported;imported.name=stringOf(node->name);imported.parent=parent;
      for(u32 c=0;c<4;++c)for(u32 r=0;r<4;++r)
        imported.localMatrix[c*4+r]=r==3?(c==3?1.f:0.f):static_cast<float>(node->node_to_parent.cols[c].v[r]);
      if(parent<0)for(u32 c=0;c<4;++c)for(u32 r=0;r<3;++r)imported.localMatrix[c*4+r]*=limits.rootScale;
      out.nodes.push_back(std::move(imported));
      for(const auto *child:node->children)self(self,child,static_cast<i32>(index));
    };
    emit(emit,scene->root_node,-1);
    std::vector<u32> remap(scene->nodes.count,UFBX_NO_INDEX);
    for(u32 index=0;index<originalOrder.size();++index)remap[originalOrder[index]]=index;
    for(auto &take:takes) {
      AnimationClip clip;clip.name=std::move(take.name);clip.duration=take.duration;clip.channels=std::move(take.channels);
      for(auto &channel:clip.channels) {
        if(channel.node>=remap.size()||remap[channel.node]==UFBX_NO_INDEX)return fail("Take FBX referencia nó fora da hierarquia publicada.");
        channel.node=remap[channel.node];
        if(out.nodes[channel.node].parent<0&&channel.path!=AnimationPath::Rotation)
          for(auto &value:channel.values)value*=limits.rootScale;
        if(!validAnimationChannel(channel))return fail("Take FBX não representável após a escala de importação.");
      }
      out.animations.push_back(std::move(clip));
    }
    out.skippedAnimations=emptyTakes;out.skippedCameras=static_cast<u32>(scene->cameras.count);out.skippedLights=static_cast<u32>(scene->lights.count);
    out.notes.push_back("Biblioteca FBX de animação: hierarquia e takes nativos, sem geometria artificial.");
    out.notes.push_back("Takes FBX: bake quaternion/TRS a "+std::to_string(bake.bakeRate)+" Hz; intervalos nomeados preservados.");
    if(std::any_of(takes.begin(),takes.end(),[](const auto &take){return take.approximatedSteps;}))
      out.notes.push_back("Descontinuidades FBX aproximadas pela taxa de bake por autorização explícita da receita.");
    return true;
  }
  for(const auto &take:takes) {
    auto encoded=jsonStream(),samplers=jsonStream(),channels=jsonStream();bool first=true;
    encoded<<"{\"name\":"<<jsonString(take.name)<<",\"samplers\":[";
    for(const auto &channel:take.channels) {
      u32 time=0,values=0;
      const bool weights=channel.path==AnimationPath::Weights;
      const char *type=weights?"SCALAR":channel.path==AnimationPath::Rotation?"VEC4":"VEC3";
      if(!data.floats(channel.times,1,"SCALAR",time)||!data.floats(channel.values,weights?1:channel.components(),type,values))
        return fail("Samplers FBX excedem o orçamento do intercâmbio.");
      const char *path=weights?"weights":channel.path==AnimationPath::Rotation?"rotation":channel.path==AnimationPath::Translation?"translation":"scale";
      if(!first){samplers<<',';channels<<',';}first=false;
      samplers<<"{\"input\":"<<time<<",\"output\":"<<values<<",\"interpolation\":\"LINEAR\"}";
      channels<<"{\"sampler\":"<<(&channel-take.channels.data())<<",\"target\":{\"node\":"<<channel.node<<",\"path\":\""<<path<<"\"}}";
    }
    encoded<<samplers.str()<<"],\"channels\":["<<channels.str()<<"]}";animations.push_back(encoded.str());
  }
  auto document=jsonStream();document<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Astra ufbx family/2\"},\"scene\":0,\"scenes\":[{\"nodes\":["<<nodeIds.at(scene->root_node)<<"]}],\"nodes\":[";
  join(document,nodes);document<<"],\"meshes\":[";join(document,meshes);document<<"],\"materials\":[";join(document,materials);
  document<<"],\"skins\":[";join(document,skins);document<<"],\"animations\":[";join(document,animations);
  document<<"],\"buffers\":[{\"byteLength\":"<<data.binary.size()<<"}],\"bufferViews\":[";join(document,data.views);document<<"],\"accessors\":[";join(document,data.accessors);document<<"]}";
  auto json=document.str();while(json.size()%4)json+=' ';while(data.binary.size()%4)data.binary.push_back(0);
  const u64 total=28u+json.size()+data.binary.size();if(total>limits.maximumBytes||total>budget||total>std::numeric_limits<u32>::max())return fail("Intercâmbio FBX excede o orçamento da importação.");
  std::vector<u8> glb;glb.reserve(static_cast<usize>(total));word(glb,0x46546c67);word(glb,2);word(glb,static_cast<u32>(total));word(glb,static_cast<u32>(json.size()));word(glb,0x4e4f534a);glb.insert(glb.end(),json.begin(),json.end());word(glb,static_cast<u32>(data.binary.size()));word(glb,0x004e4942);glb.insert(glb.end(),data.binary.begin(),data.binary.end());
  const auto skippedCameras=scene->cameras.count,skippedLights=scene->lights.count;
  const bool legacy=std::any_of(scene->materials.begin(),scene->materials.end(),[](const auto *m){return !m->features.pbr.enabled;});
  scene.reset();data={};nodes.clear();meshes.clear();materials.clear();
  auto interchangeLimits=limits;
  if(recalculateTangents)interchangeLimits.tangents=GltfTangentsCalculate;
  if(!importGlb(glb,interchangeLimits,progress,out))return false;
  if(out.skippedSkins||out.unsupportedAnimationChannels||out.skippedAnimations)
    return fail("Consumidor glTF recusou skin/canal FBX preparado; publicação parcial de deformação recusada.");
  out.skippedAnimations+=emptyTakes;
  out.skippedCameras+=static_cast<u32>(skippedCameras);out.skippedLights+=static_cast<u32>(skippedLights);out.skippedTextures+=static_cast<u32>(omittedTextures.size());
  if(!omittedTextures.empty())out.notes.push_back("Texturas FBX ainda não vinculadas neste perfil; materiais Unity externos exigem resolução do pacote.");
  if(legacy)out.appearanceExtensions.push_back("FBX Phong/Lambert: somente cor difusa/emissão; resposta especular original não reproduzida.");
  if(droppedTangents)out.appearanceExtensions.push_back("Base de tangentes FBX degenerada: tangentes do modelo recalculadas a partir da UV pelo preparador nativo onde disponível.");
  if(reducedWeights)out.notes.push_back("Skin FBX: quatro maiores influências renormalizadas por autorização da receita.");
  if(linearFromDualQuaternion)out.notes.push_back("Skin FBX marcado como dual quaternion: deformado em mistura linear com os mesmos pesos, como na Unity.");
  if(std::any_of(takes.begin(),takes.end(),[](const auto &take){return take.approximatedSteps;}))
    out.notes.push_back("Descontinuidades FBX aproximadas pela taxa de bake por autorização explícita da receita.");
  if(!takes.empty())out.notes.push_back("Takes FBX: bake quaternion/TRS/morph a "+std::to_string(bake.bakeRate)+" Hz; intervalos nomeados preservados.");
  out.notes.push_back("FBX nativo: eixos destros +Y, metros, pivôs e hierarquia preservados; UV convertida para origem glTF.");
  return true;
}
} // namespace ae::resources
