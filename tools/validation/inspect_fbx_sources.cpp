// Private-corpus research tool, not an engine import route. Output contains
// local source names and must remain in ignored local-assets/.
#include "resources/fbx_deformation.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "third_party/ufbx/ufbx.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

int main(int argc,char **argv) {
  if(argc!=2)return 2;
  using namespace ae;
  resources::FbxDeformationPolicy policy;policy.reduceWeightsToFour=true;policy.allowStepApproximation=true;
  u32 files=0,loaded=0,skinned=0,animated=0,meshless=0,morphs=0,constraints=0,skinReady=0,takesReady=0;
  std::error_code error;std::filesystem::recursive_directory_iterator it(argv[1],error),end;
  if(error)return 2;
  std::printf("source\tnodes\tmeshes\tskins\ttakes\tmorphs\tconstraints\tskinPrepared\ttakesPrepared\tkeys\tdiagnostic\n");
  for(;it!=end;it.increment(error)) {
    if(error)return 2;
    if(!it->is_regular_file())continue;
    auto extension=it->path().extension().string();for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(extension!=".fbx")continue;
    ++files;const auto size=it->file_size();const auto name=it->path().lexically_relative(argv[1]).generic_string();
    if(size>256ull<<20){std::printf("%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\tencoded budget\n",name.c_str());continue;}
    std::ifstream file(it->path(),std::ios::binary);std::vector<u8> bytes(static_cast<usize>(size));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size)))return 2;
    ufbx_load_opts options{};options.temp_allocator.memory_limit=128u<<20;options.result_allocator.memory_limit=128u<<20;
    options.file_format=UFBX_FILE_FORMAT_FBX;options.strict=true;options.node_depth_limit=128;
    options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;
    options.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    options.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
    options.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_COMPENSATE;
    options.pivot_handling=UFBX_PIVOT_HANDLING_ADJUST_TO_PIVOT;options.generate_missing_normals=true;
    ufbx_error failure{};
    std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(ufbx_load_memory(bytes.data(),bytes.size(),&options,&failure),ufbx_free_scene);
    if(!scene){char detail[512]{};ufbx_format_error(detail,sizeof(detail),&failure);for(auto &c:detail)if(c=='\n'||c=='\r'||c=='\t')c=' ';std::printf("%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t%s\n",name.c_str(),detail);continue;}
    ++loaded;skinned+=scene->skin_deformers.count!=0;animated+=scene->anim_stacks.count!=0;meshless+=scene->meshes.count==0;
    morphs+=scene->blend_deformers.count!=0;constraints+=scene->constraints.count!=0;
    bool skinOk=true,takesOk=true;std::string diagnostic;
    for(const auto *mesh:scene->meshes)if(mesh->skin_deformers.count) {
      resources::FbxSkinAttributes attributes;std::string detail;
      if(!resources::prepareFbxSkin(*mesh,*scene,policy,{},attributes,detail)){skinOk=false;diagnostic+=detail;}
    }
    std::vector<resources::FbxBakedTake> takes;std::string detail;u64 keys=0;
    if(scene->anim_stacks.count&&!resources::prepareFbxTakes(*scene,policy,{},takes,detail)){takesOk=false;diagnostic+=detail;}
    for(const auto &take:takes)for(const auto &channel:take.channels)keys+=channel.times.size();
    skinReady+=skinOk&&scene->skin_deformers.count!=0;takesReady+=takesOk&&scene->anim_stacks.count!=0;
    std::printf("%s\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%d\t%d\t%llu\t%s\n",name.c_str(),scene->nodes.count,scene->meshes.count,
      scene->skin_deformers.count,scene->anim_stacks.count,scene->blend_deformers.count,scene->constraints.count,skinOk,takesOk,
      static_cast<unsigned long long>(keys),diagnostic.c_str());std::fflush(stdout);
  }
  std::fprintf(stderr,"FBX_CORPUS files=%u loaded=%u skinned=%u animated=%u meshless=%u morphs=%u constraints=%u skinPrepared=%u takesPrepared=%u\n",
    files,loaded,skinned,animated,meshless,morphs,constraints,skinReady,takesReady);
  return loaded==files?0:1;
}
