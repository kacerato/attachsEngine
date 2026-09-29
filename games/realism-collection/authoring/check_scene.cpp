#include "editor/editor_archive.h"
#include "runtime/scene_physics.h"
#include "scene/environment.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
int main(int argc,char**argv){
  if(argc==7 && std::string(argv[1])=="--mechanism") {
    std::ifstream input(argv[2]);std::ostringstream text;text<<input.rdbuf();
    ae::editor::EditorDocument document;
    if(!ae::editor::deserializeEditorDocument(text.str(),0,document))return 1;
    ae::runtime::GameWorld world;ae::runtime::ScenePhysics physics;
    if(!world.load(document)||!physics.start(world))return 2;
    const auto object=argv[3][0]=='#'?world.handle(std::stoul(argv[3]+1)):world.findChildByName(world.root(),argv[3],true);
    if(!world.alive(object))return 3;
    const float target=std::stof(argv[4]),mass=std::stof(argv[5]),spring=std::stof(argv[6]);
    ae::runtime::Transform pose;float velocity[3],load=0;
    for(int frame=0;frame<900;++frame) {
      world.worldTransform(object,pose);physics.getBodyVelocity(object.id,velocity);
      const auto error=target-pose.position[1];load=std::clamp(load+error*spring*.4f/60,-8000.0f,8000.0f);
      const float force[3]{0,error*spring-velocity[1]*400+mass*9.81f+load,0};
      physics.applyBodyForce(object.id,force,0);physics.advance(1.0/60.0,world);
    }
    world.worldTransform(object,pose);
    std::printf("MECHANISM %s target=%.2f actual=%.2f\n",argv[3],target,pose.position[1]);
    return std::abs(target-pose.position[1])<.2f?0:4;
  }
  if(argc>=3 && std::string(argv[1])=="--environment") {
    ae::scene::Environment environment;
    auto &v=environment.values;
    v.fog=false;v.bloom=true;v.filmGrain=false;v.ambientOcclusion=true;
    v.sky=ae::renderer::SkyModel::Atmosphere;v.toneMapper=ae::renderer::ToneMapper::Aces;
    v.exposureEv=.7f;v.indirectDiffuse=1.0f;v.indirectSpecular=.4f;
    v.bloomIntensity=.025f;v.contrast=1.05f;v.saturation=1.0f;
    const std::string theme=argc>3?argv[3]:"usina";
    v.fog=true;v.fogDensity=.006f;v.fogStart=14;v.fogHeightFalloff=.2f;
    v.fogColor[0]=.16f;v.fogColor[1]=.19f;v.fogColor[2]=.21f;
    if(theme=="quarry") {
      v.hdriExposureEv=3; // prepare_hdri.py records the matching inverse source gain.
      v.exposureEv=-.35f;v.indirectDiffuse=.8f;v.indirectSpecular=.65f;
      v.fogDensity=.004f;v.fogStart=28;v.fogHeightFalloff=.08f;
      v.fogColor[0]=.48f;v.fogColor[1]=.40f;v.fogColor[2]=.29f;
      v.skyHorizon[0]=.52f;v.skyHorizon[1]=.48f;v.skyHorizon[2]=.39f;
      v.saturation=.93f;
    } else if(theme=="tomb") {
      v.exposureEv=.3f;v.indirectDiffuse=.65f;v.indirectSpecular=.3f;
      v.fogDensity=.008f;v.fogStart=12;v.fogHeightFalloff=.16f;
      v.fogColor[0]=.22f;v.fogColor[1]=.18f;v.fogColor[2]=.13f;
      v.skyHorizon[0]=.55f;v.skyHorizon[1]=.43f;v.skyHorizon[2]=.27f;
    }
    std::ofstream output(argv[2]);environment.write(output);return output.good()?0:1;
  }
  for(int i=1;i<argc;++i){
    std::ifstream file(argv[i],std::ios::binary);std::ostringstream buffer;buffer<<file.rdbuf();
    ae::editor::EditorDocument document;
    if(!ae::editor::deserializeEditorDocument(buffer.str(),0,document)){std::printf("ARCHIVE FAIL %s\n",argv[i]);return 1;}
    ae::runtime::GameWorld world;ae::runtime::ScenePhysics physics;
    if(!world.load(document)||!physics.start(world)){std::printf("PHYSICS FAIL %s: %s\n",argv[i],physics.error().c_str());return 2;}
    for(int f=0;f<120;++f)if(!physics.advance(1.0/60.0,world)){std::printf("STEP FAIL\n");return 3;}
    std::printf("PASS archive and 120 Jolt steps: %s bodies=%u (without C# gameplay)\n",argv[i],physics.bodyCount());
  }
}
