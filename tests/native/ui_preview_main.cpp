#include "mechanisms_fixture.h"
#include "scene/physics2d_components.h"
#include "scene/audio.h"
#include "scene/event_connection.h"
#include "scene/camera.h"
#include "scene/virtual_camera.h"
#include "scene/audio.h"
#include "scene/audio_mixer.h"
#include "scene/collider.h"
#include "scene/physics_body.h"
#include "scene/timer.h"
#include <tuple>
#include <sstream>
#include <iomanip>
#include <functional>
#include "editor/editor_route_component.h"
#include "editor/editor_reference_picker.h"
// Renderiza a tela do editor em um arquivo, sem GPU e sem aparelho.
//
// A interface é desenhada pela engine. Sem isto, a única forma de ver se um
// painel ficou onde deveria seria montar um APK, instalar e olhar — um ciclo de
// minutos para um ajuste de oito pixels. Aqui o ciclo é de segundos, e o que
// rasteriza é o espelho do fragment shader (tests/native/ui_software_raster.h),
// então o que aparece aqui é o que a GPU vai desenhar.
//
// Não faz parte do produto: é um alvo de ferramenta, ao lado dos testes.
//
// Uso:
//   aether_ui_preview [saida.ppm] [largura] [altura]
#include "editor/editor_history.h"
#include "editor/editor_code_workspace.h"
#include "editor/editor_value_library.h"
#include "editor/editor_curve_view.h"
#include "scene/script_behavior.h"
#include "scene/camera_follow.h"
#include "scene/animation.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_creation_catalog.h"
#include "renderer/water_authoring_geometry.h"
#include "runtime/scene_tweens.h"
#include "runtime/scene_paths.h"
#include "scene/path.h"
#include "scene/path_follow.h"
#include "ui_software_raster.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <limits>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cmath>

using namespace ae;

namespace {

bool readAsset(const char *relative, std::vector<u8> &out) {
  const std::string path = std::string(AETHER_REPOSITORY_ROOT) + "/" + relative;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    std::fprintf(stderr, "nao abriu %s\n", path.c_str());
    return false;
  }
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  out.resize(size > 0 ? static_cast<usize>(size) : 0);
  const bool ok = size > 0 && std::fread(out.data(), 1, out.size(), file) == out.size();
  std::fclose(file);
  return ok;
}

bool writePpm(const char *path, const test::UiSoftwareTarget &target) {
  std::FILE *file = std::fopen(path, "wb");
  if (file == nullptr) return false;
  std::fprintf(file, "P6\n%u %u\n255\n", target.width, target.height);
  std::vector<u8> row(static_cast<usize>(target.width) * 3);
  for (u32 y = 0; y < target.height; ++y) {
    for (u32 x = 0; x < target.width; ++x) {
      const usize source = (static_cast<usize>(y) * target.width + x) * 4;
      for (u32 channel = 0; channel < 3; ++channel) {
        const float value = target.pixels[source + channel];
        row[static_cast<usize>(x) * 3 + channel] =
            static_cast<u8>((value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value)) * 255.0f + 0.5f);
      }
    }
    std::fwrite(row.data(), 1, row.size(), file);
  }
  std::fclose(file);
  return true;
}

// A genuine short mono PCM16 WAV; the fixture uses the product importer.
bool writePreviewWave(const std::filesystem::path &path) {
  constexpr u32 frames=4800,rate=48000;
  std::vector<u8> bytes;
  const auto tag=[&](const char *text){for(u32 i=0;i<4;++i) bytes.push_back(static_cast<u8>(text[i]));};
  const auto integer=[&](u32 value,u32 count){for(u32 i=0;i<count;++i) bytes.push_back(static_cast<u8>(value>>(i*8)));};
  tag("RIFF");integer(36+frames*2,4);tag("WAVE");tag("fmt ");integer(16,4);
  integer(1,2);integer(1,2);integer(rate,4);integer(rate*2,4);integer(2,2);integer(16,2);
  tag("data");integer(frames*2,4);
  for(u32 i=0;i<frames;++i) integer(static_cast<u16>(static_cast<i16>(std::sin(double(i)*6.283185307179586*440/rate)*4000)),2);
  auto *file=std::fopen(path.string().c_str(),"wb");if(!file) return false;
  const bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);return ok;
}

// Aceite no aparelho do Material físico (docs/planos/MATERIAL-FISICO-2026-10-06.md):
// materiais criados pelo fluxo do editor a partir de dois corpos, chão sem quique.
int writePhysicsMaterialProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Physics material export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/physics-material/MaterialProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes",".astra"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/MaterialProbe.cs",probe)) return fail("cannot publish source");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  auto &document=session.document();
  const auto cameraId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Câmera");auto camera=*document.find(cameraId);
  camera.transform.position[1]=1;camera.transform.position[2]=-8;
  if(!camera.components.add(scene::Camera::descriptor)||!document.applyEntityValues(cameraId,camera)) return fail("camera");
  const auto floorId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Chão");auto floor=*document.find(floorId);
  floor.transform.position[1]=-.5f;
  auto *floorBody=static_cast<scene::PhysicsBody*>(floor.components.add(scene::PhysicsBody::descriptor));floorBody->restitution=0;
  auto *floorShape=static_cast<scene::Collider*>(floor.components.add(scene::Collider::descriptor));floorShape->halfX=floorShape->halfZ=5;floorShape->halfY=.5f;
  if(!document.applyEntityValues(floorId,floor)) return fail("floor");
  const auto ball=[&](const char *name,float x,u32 combine,u64 &instance){
    const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto v=*document.find(id);
    v.transform.position[0]=x;v.transform.position[1]=2;
    auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));
    b->motion=scene::BodyMotion::Dynamic;b->restitution=.9f;b->restitutionCombine=combine;instance=b->instanceId();
    auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->shape=scene::ColliderShape::Sphere;c->radius=.5f;
    document.applyEntityValues(id,v);return id;};
  u64 bouncyInstance=0,dullInstance=0;
  const auto bouncy=ball("Bola de borracha",-1.5f,4,bouncyInstance),dull=ball("Bola de massa",1.5f,2,dullInstance);
  std::string diagnostic;
  if(!session.createPhysicsMaterial(bouncy,bouncyInstance,diagnostic).valid()) return fail(diagnostic);
  if(!session.createPhysicsMaterial(dull,dullInstance,diagnostic).valid()) return fail(diagnostic);
  const auto probeId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Sonda");auto probeValue=*document.find(probeId);
  auto *script=static_cast<scene::ScriptBehavior*>(probeValue.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="acceptance.physics_material";script->source="Scripts/MaterialProbe.cs";
  if(!document.applyEntityValues(probeId,probeValue)) return fail("probe");
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  if(!editor::EditorImportTransaction::writeText(root/".astra/assets.astra",session.serializeAssets())) return fail("registry");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"MaterialFisico-20261006\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":2},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Physics material project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho do Tween de propriedade (docs/planos/SEQUENCIA-DE-TWEENS-2026-10-06.md).
int writePropertyTweenProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Property tween export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/property-tween/PropertyProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/PropertyProbe.cs",probe)) return fail("cannot publish source");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  auto &document=session.document();
  const auto cameraId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Câmera");auto camera=*document.find(cameraId);
  camera.transform.position[1]=1;camera.transform.position[2]=-6;
  if(!camera.components.add(scene::Camera::descriptor)||!document.applyEntityValues(cameraId,camera)) return fail("camera");
  const auto stageId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Luz do palco");auto stage=*document.find(stageId);
  static_cast<scene::Light*>(stage.components.add(scene::Light::descriptor))->intensity=0;
  if(!document.applyEntityValues(stageId,stage)) return fail("stage light");
  const auto lightUpId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Acender palco");auto lightUp=*document.find(lightUpId);
  auto *fade=static_cast<scene::PropertyTween*>(lightUp.components.add(scene::PropertyTween::descriptor));
  fade->target=stageId;fade->componentType="astra.render.light";fade->property="intensity";fade->destination=50;fade->duration=1;
  if(!document.applyEntityValues(lightUpId,lightUp)) return fail("fade");
  const auto spotId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Holofote");auto spot=*document.find(spotId);
  static_cast<scene::Light*>(spot.components.add(scene::Light::descriptor))->intensity=0;
  auto *move=static_cast<scene::TransformTween*>(spot.components.add(scene::TransformTween::descriptor));move->autoplay=false;move->duration=.5f;move->destination[0]=1;
  auto *glow=static_cast<scene::PropertyTween*>(spot.components.add(scene::PropertyTween::descriptor));
  glow->autoplay=false;glow->componentType="astra.render.light";glow->property="intensity";glow->destination=30;glow->duration=.8f;
  if(!document.applyEntityValues(spotId,spot)) return fail("spot");
  const auto showId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Show");auto show=*document.find(showId);
  static_cast<scene::TweenSequence*>(show.components.add(scene::TweenSequence::descriptor))->steps[0]={spotId,false,.2f};
  auto *script=static_cast<scene::ScriptBehavior*>(show.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="acceptance.property_tween";script->source="Scripts/PropertyProbe.cs";
  if(!document.applyEntityValues(showId,show)) return fail("show");
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"TweenPropriedade-20261006\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Property tween project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho do bloco F (docs/planos/FISICA-QUEBRA-E-PERSONAGEM-2026-10-07.md,
// MATERIAL-FISICO-2026-10-06.md): material por forma, superfície, quebra de
// junta, colisão do personagem, Raio e Braço de mola numa só cena.
int writePhysicsFProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Physics F export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/physics-f/PhysicsFProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes",".astra"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/PhysicsFProbe.cs",probe)) return fail("cannot publish source");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  auto &document=session.document();
  const auto object=[&](const char *name,runtime::ObjectId parent,float x,float y,float z){
    const auto id=document.createEntity(parent,runtime::ObjectKind::Folder,name);auto v=*document.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;document.applyEntityValues(id,v);return id;};
  const auto body=[&](runtime::ObjectId id,scene::BodyMotion motion,float mass,u32 surface){
    auto v=*document.find(id);auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));
    b->motion=motion;b->mass=mass;b->surface=surface;document.applyEntityValues(id,v);};
  const auto box=[&](runtime::ObjectId id,float hx,float hy,float hz,runtime::ObjectId owner){
    auto v=*document.find(id);auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
    c->halfX=hx;c->halfY=hy;c->halfZ=hz;c->owner=owner;document.applyEntityValues(id,v);return c->instanceId();};
  const auto cameraId=object("Câmera",document.root(),0,4,-14);
  {auto v=*document.find(cameraId);v.components.add(scene::Camera::descriptor);document.applyEntityValues(cameraId,v);}
  // Chão composto: corpo de concreto, metade esquerda com material próprio de borracha.
  const auto floor=object("Chão",document.root(),0,-.5f,0);body(floor,scene::BodyMotion::Static,1,1);
  const auto leftPart=object("Chão esquerdo",floor,-6,0,0);const auto leftShape=box(leftPart,6,.5f,10,floor);
  const auto rightPart=object("Chão direito",floor,6,0,0);box(rightPart,6,.5f,10,floor);
  {auto v=*document.find(leftPart);auto *own=static_cast<scene::Collider*>(v.components.editInstance(leftShape));
   own->ownMaterial=true;own->restitution=.95f;own->restitutionCombine=4;own->surface=9;document.applyEntityValues(leftPart,v);}
  std::string diagnostic;
  if(!session.createPhysicsMaterial(leftPart,leftShape,diagnostic).valid()) return fail(diagnostic);
  for(const auto &[name,x]:{std::pair{"Bola esquerda",-3.f},std::pair{"Bola direita",3.f}}) {
    const auto id=object(name,document.root(),x,2.5f,-2);body(id,scene::BodyMotion::Dynamic,1,0);
    auto v=*document.find(id);auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));
    c->shape=scene::ColliderShape::Sphere;c->radius=.4f;document.applyEntityValues(id,v);
  }
  // Junta que quebra: 10 kg pendurados num limite de 50 N.
  const auto anchor=object("Âncora",document.root(),0,6,-6);body(anchor,scene::BodyMotion::Static,1,0);box(anchor,.25f,.25f,.25f,0);
  const auto weight=object("Peso",document.root(),0,5,-6);body(weight,scene::BodyMotion::Dynamic,10,0);box(weight,.25f,.25f,.25f,0);
  {auto v=*document.find(weight);auto *j=static_cast<scene::Joint*>(v.components.add(scene::Joint::descriptor));
   j->kind=scene::JointKind::Fixed;j->connectedBody=anchor;j->breakForce=50;document.applyEntityValues(weight,v);}
  // Personagem andando contra a parede.
  const auto wall=object("Parede",document.root(),8,1,0);body(wall,scene::BodyMotion::Static,1,0);box(wall,.25f,2,3,0);
  const auto actor=object("Personagem",document.root(),5,1,0);
  {auto v=*document.find(actor);static_cast<scene::Character*>(v.components.add(scene::Character::descriptor))->speed=3;document.applyEntityValues(actor,v);}
  // Raio de chão sobre a metade de borracha e braço de mola contra outra parede.
  const auto sensor=object("Sensor de chão",document.root(),-3,2,3);
  {auto v=*document.find(sensor);static_cast<scene::RayCast*>(v.components.add(scene::RayCast::descriptor))->target[1]=-5;document.applyEntityValues(sensor,v);}
  const auto back=object("Parede do braço",document.root(),0,1,9);body(back,scene::BodyMotion::Static,1,0);box(back,3,2,.25f,0);
  const auto armId=object("Braço",document.root(),0,1,6);
  {auto v=*document.find(armId);static_cast<scene::SpringArm*>(v.components.add(scene::SpringArm::descriptor))->length=6;document.applyEntityValues(armId,v);}
  object("Câmera do braço",armId,0,0,6);
  // Sensor/Área: zona com visitante e um corpo não detectável.
  const auto zone=object("Zona",document.root(),-9,1,-8);
  {auto v=*document.find(zone);auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->sensor=true;
   auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=c->halfY=c->halfZ=1.5f;document.applyEntityValues(zone,v);}
  for(const auto &[name,x,monitorable]:{std::tuple{"Visitante",-9.4f,true},std::tuple{"Fantasma",-8.6f,false}}) {
    const auto id=object(name,document.root(),x,2,-8);auto v=*document.find(id);
    auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;b->monitorable=monitorable;
    auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->shape=scene::ColliderShape::Sphere;c->radius=.3f;document.applyEntityValues(id,v);
  }
  // Centro de massa: a mesma caixa alta, com e sem o centro deslocado para o lado.
  for(const auto &[name,x,shifted]:{std::tuple{"Caixa equilibrada",3.f,false},std::tuple{"Caixa desequilibrada",5.f,true}}) {
    const auto id=object(name,document.root(),x,1.2f,6);auto v=*document.find(id);
    auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;
    if(shifted){b->automaticCenterOfMass=false;b->centerOfMass[0]=1.5f;}
    auto *c=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));c->halfX=.3f;c->halfY=1;c->halfZ=.3f;document.applyEntityValues(id,v);
  }
  {const auto id=object("Caixa interpolada",document.root(),-4,3,6);auto v=*document.find(id);
   auto *b=static_cast<scene::PhysicsBody*>(v.components.add(scene::PhysicsBody::descriptor));b->motion=scene::BodyMotion::Dynamic;b->interpolation=1;
   v.components.add(scene::Collider::descriptor);document.applyEntityValues(id,v);}
  const auto probeId=object("Sonda",document.root(),0,0,0);
  {auto v=*document.find(probeId);auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
   script->scriptType="acceptance.physics_f";script->source="Scripts/PhysicsFProbe.cs";document.applyEntityValues(probeId,v);}
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  if(!editor::EditorImportTransaction::writeText(root/".astra/assets.astra",session.serializeAssets())) return fail("registry");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"FisicaF-20261007\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":1},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Physics F project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho do bloco G (docs/planos/CAMERA-VIRTUAL-2026-10-07.md): Cérebro
// na câmera principal, órbita no jogador atrás de uma parede e câmera aérea que
// a sonda promove e rebaixa por prioridade.
int writeVirtualCameraProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Virtual camera export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/virtual-camera/VirtualCameraProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/VirtualCameraProbe.cs",probe)) return fail("cannot publish source");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  auto &document=session.document();
  const auto object=[&](const char *name,float x,float y,float z){
    const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto v=*document.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;document.applyEntityValues(id,v);return id;};
  const auto mainId=object("Câmera principal",0,3,-8);
  {auto v=*document.find(mainId);v.components.add(scene::Camera::descriptor);
   auto *brain=static_cast<scene::CameraBrain*>(v.components.add(scene::CameraBrain::descriptor));
   brain->defaultBlend=scene::CameraBlendStyle::EaseInOut;brain->defaultBlendTime=1;
   if(!document.applyEntityValues(mainId,v)) return fail("main camera");}
  const auto playerId=object("Jogador",0,0,0);
  const auto orbitId=object("Câmera de órbita",0,2,-6);
  {auto v=*document.find(orbitId);auto *c=static_cast<scene::VirtualCamera*>(v.components.add(scene::VirtualCamera::descriptor));
   c->priority=10;c->trackingTarget=playerId;c->position=scene::VirtualCameraPosition::Orbit;c->rotation=scene::VirtualCameraRotation::LookAt;
   c->orbitRadius=6;c->orbitPitch=15;c->aimOffset[1]=1;c->avoidObstacles=true;c->cameraRadius=.2f;c->minimumDistance=1;
   c->noiseAmplitude=.4f;c->noiseFrequency=.5f;
   if(!document.applyEntityValues(orbitId,v)) return fail("orbit camera");}
  const auto aerialId=object("Câmera aérea",8,10,8);
  {auto v=*document.find(aerialId);auto *c=static_cast<scene::VirtualCamera*>(v.components.add(scene::VirtualCamera::descriptor));
   c->priority=0;c->lookAtTarget=playerId;c->rotation=scene::VirtualCameraRotation::LookAt;c->verticalFov=45;
   if(!document.applyEntityValues(aerialId,v)) return fail("aerial camera");}
  const auto probeId=object("Sonda",0,0,0);
  {auto v=*document.find(probeId);auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
   script->scriptType="acceptance.virtual_camera";script->source="Scripts/VirtualCameraProbe.cs";
   if(!document.applyEntityValues(probeId,v)) return fail("probe");}
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"CameraVirtual-20261007\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Virtual camera project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho do bloco H (docs/planos/MIXER-DE-AUDIO-2026-10-07.md):
// buses com efeitos, envio para reverberação, ducking por sidechain, WAV longo
// em streaming e Snapshot, com sons sintetizados aqui mesmo.
namespace {
std::vector<u8> synthesizedWave(u32 frames,const std::function<float(u32,u32)> &sample) {
  std::vector<u8> bytes(44+usize(frames)*4,0);
  const auto put16=[&](usize p,u32 v){bytes[p]=u8(v);bytes[p+1]=u8(v>>8);};
  const auto put32=[&](usize p,u32 v){for(u32 k=0;k<4;++k)bytes[p+k]=u8(v>>(k*8));};
  std::memcpy(bytes.data(),"RIFF",4);put32(4,u32(bytes.size()-8));std::memcpy(bytes.data()+8,"WAVEfmt ",8);
  put32(16,16);put16(20,1);put16(22,2);put32(24,48000);put32(28,192000);put16(32,4);put16(34,16);std::memcpy(bytes.data()+36,"data",4);put32(40,frames*4);
  for(u32 i=0;i<frames;++i) for(u32 ch=0;ch<2;++ch) {
    const float v=std::clamp(sample(i,ch),-1.f,1.f);put16(44+(usize(i)*2+ch)*2,u32(u16(i16(std::lround(v*32767)))));
  }
  return bytes;
}
}
int writeAudioMixerProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Audio mixer export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/audio-mixer/AudioMixerProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes",".astra","Audio"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/AudioMixerProbe.cs",probe)) return fail("cannot publish source");
  constexpr float tau=6.2831853f;
  // Música: acorde lá maior com tremolo, 4 s, laço entre 1 s e 3 s.
  const auto musicWave=synthesizedWave(4*48000,[&](u32 i,u32){const float t=float(i)/48000;
    return (.16f*std::sin(tau*220*t)+.12f*std::sin(tau*277.18f*t)+.12f*std::sin(tau*329.63f*t))*(.85f+.15f*std::sin(tau*3*t));});
  // Fala: sílabas de 180 ms com vibrato, 2 s.
  const auto speechWave=synthesizedWave(2*48000,[&](u32 i,u32){const float t=float(i)/48000;const float syllable=std::fmod(t,.24f);
    return syllable<.18f?.45f*std::sin(tau*(320+30*std::sin(tau*6*t))*t)*std::sin(3.14159f*syllable/.18f):0.f;});
  // Ambiente: 50 s (acima do orçamento de memória): pede streaming.
  const auto ambientWave=synthesizedWave(50*48000,[&](u32 i,u32 ch){const float t=float(i)/48000;
    return .12f*std::sin(tau*82.4f*t+ch)+.08f*std::sin(tau*123.5f*t)*(.6f+.4f*std::sin(tau*.2f*t))+.05f*std::sin(tau*164.8f*t+2*ch);});
  for(const auto &[name,bytes]:{std::pair{"Audio/musica.wav",&musicWave},std::pair{"Audio/fala.wav",&speechWave},std::pair{"Audio/ambiente-longo.wav",&ambientWave}})
    if(!editor::EditorImportTransaction::write(root/name,*bytes)) return fail("cannot write wave");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  resources::AssetGuid musicClip,speechClip,ambientClip;std::string error;
  if(!session.importWaveClip("Audio/musica.wav",musicClip,error)||!session.importWaveClip("Audio/fala.wav",speechClip,error)||
     !session.importWaveClip("Audio/ambiente-longo.wav",ambientClip,error)) return fail(error);
  auto &document=session.document();
  const auto object=[&](const char *name,float x,float y,float z){
    const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto v=*document.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;document.applyEntityValues(id,v);return id;};
  const auto cameraId=object("Câmera",0,1.6f,-4);
  {auto v=*document.find(cameraId);v.components.add(scene::Camera::descriptor);v.components.add(scene::AudioListener::descriptor);document.applyEntityValues(cameraId,v);}
  const auto voices=object("Falas",0,0,0),reverb=object("Reverb",0,0,0),musicBus=object("Música",0,0,0),ambientBus=object("Ambiente",0,0,0);
  {auto v=*document.find(voices);v.components.add(scene::AudioBus::descriptor);document.applyEntityValues(voices,v);}
  {auto v=*document.find(reverb);v.components.add(scene::AudioBus::descriptor);auto *r=static_cast<scene::AudioReverb*>(v.components.add(scene::AudioReverb::descriptor));
   r->dry=0;r->wet=.6f;document.applyEntityValues(reverb,v);}
  {auto v=*document.find(musicBus);static_cast<scene::AudioBus*>(v.components.add(scene::AudioBus::descriptor))->volume=.9f;
   static_cast<scene::AudioFilter*>(v.components.add(scene::AudioFilter::descriptor))->cutoff=20000;
   auto *c=static_cast<scene::AudioCompressor*>(v.components.add(scene::AudioCompressor::descriptor));c->sidechain=voices;c->threshold=-35;c->ratio=8;c->attack=10;c->release=400;
   auto *s=static_cast<scene::AudioSend*>(v.components.add(scene::AudioSend::descriptor));s->target=reverb;s->level=.3f;
   document.applyEntityValues(musicBus,v);}
  {auto v=*document.find(ambientBus);v.components.add(scene::AudioBus::descriptor);document.applyEntityValues(ambientBus,v);}
  const auto source=[&](const char *name,float x,resources::AssetGuid clip,u64 bus,const std::function<void(scene::AudioSource&)> &setup){
    const auto id=object(name,x,0,0);auto v=*document.find(id);auto *s=static_cast<scene::AudioSource*>(v.components.add(scene::AudioSource::descriptor));
    s->clip=clip;s->bus=bus;setup(*s);document.applyEntityValues(id,v);return id;};
  source("Fonte da música",0,musicClip,musicBus,[](scene::AudioSource &s){s.loop=true;s.loopStart=1;s.loopEnd=3;s.priority=10;});
  source("Fala",0,speechClip,voices,[](scene::AudioSource &s){s.playback=scene::AudioPlayback::Stopped;s.volume=.9f;s.priority=0;});
  source("Fonte do ambiente",5,ambientClip,ambientBus,[](scene::AudioSource &s){s.loop=true;s.loading=scene::AudioLoading::Stream;
    s.dimension=scene::AudioDimension::Spatial;s.spatialBlend=.5f;s.minDistance=2;s.priority=200;});
  const auto pause=object("Pausa",0,0,0);
  {auto v=*document.find(pause);auto *n=static_cast<scene::AudioSnapshot*>(v.components.add(scene::AudioSnapshot::descriptor));
   n->slots[0]={musicBus,scene::AudioSnapshotParameter::BusGain,.3f};n->slots[1]={musicBus,scene::AudioSnapshotParameter::FilterCutoff,800};
   document.applyEntityValues(pause,v);}
  const auto probeId=object("Sonda",0,0,0);
  {auto v=*document.find(probeId);auto *script=static_cast<scene::ScriptBehavior*>(v.components.add(scene::ScriptBehavior::descriptor));
   script->scriptType="acceptance.audio_mixer";script->source="Scripts/AudioMixerProbe.cs";document.applyEntityValues(probeId,v);}
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  if(!editor::EditorImportTransaction::writeText(root/".astra/assets.astra",session.serializeAssets())) return fail("registry");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"MixerAudio-20261007\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":3},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Audio mixer project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho do bloco D (docs/planos/SEQUENCIA-DE-TWEENS-2026-10-06.md):
// Porta sobe (relativo), depois Luz e Placa juntas após 0,3 s; duas passagens.
int writeSequenceProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Sequence export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe;
  if(!readAsset("tests/fixtures/tween-sequence/SequenceProbe.cs",probe)) return fail("fixture source unavailable");
  for(const auto *folder:{"Scripts","scenes"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/SequenceProbe.cs",probe)) return fail("cannot publish source");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  auto &document=session.document();
  const auto cameraId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Câmera");auto camera=*document.find(cameraId);
  camera.transform.position[1]=1;camera.transform.position[2]=-6;
  if(!camera.components.add(scene::Camera::descriptor)||!document.applyEntityValues(cameraId,camera)) return fail("camera");
  const auto tweened=[&](const char *name,float x,bool relative){
    const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto v=*document.find(id);v.transform.position[0]=x;
    auto *t=static_cast<scene::TransformTween*>(v.components.add(scene::TransformTween::descriptor));
    t->autoplay=false;t->duration=.5f;t->relative=relative;t->destination[0]=relative?0:x;t->destination[1]=1;
    document.applyEntityValues(id,v);return id;};
  const auto door=tweened("Porta",-2,true),lamp=tweened("Luz do corredor",0,false),sign=tweened("Placa",2,false);
  const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Abertura da sala");auto value=*document.find(id);
  auto *sequence=static_cast<scene::TweenSequence*>(value.components.add(scene::TweenSequence::descriptor));
  sequence->steps[0]={door,false,0};sequence->steps[1]={lamp,false,.3f};sequence->steps[2]={sign,true,0};sequence->loops=2;
  auto *script=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="acceptance.tween_sequence";script->source="Scripts/SequenceProbe.cs";
  if(!session.history().applyValues(document,id,value)) return fail("sequence");
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))) return fail("scene");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"Sequencia-20261006\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Sequence project written: %s\n",root.generic_string().c_str());
  return 0;
}
// Aceite no aparelho dos blocos B, C e C2 (docs/planos/CONEXOES-DE-EVENTO,
// SERVICOS-RUNTIME e CENAS-EM-PLAY): cena com câmera autorada, alvo, sensor
// com Gatilho sonoro (Conexão de evento -> AudioSource.play) e corpo caindo;
// segunda cena "Fase2" para a troca por script. Mesmo importador de WAV e
// mesmo arquivo de cena do produto.
int writeServicesProject(const char *directory) {
  namespace fs=std::filesystem;
  const auto fail=[](const std::string &message){std::fprintf(stderr,"Services export refused: %s\n",message.c_str());return 2;};
  if(!directory||!directory[0]) return fail("provide a new empty output directory");
  std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
  if(ec||(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))) return fail("output must be a new empty directory");
  std::vector<u8> probe,fase2,wave;
  if(!readAsset("tests/fixtures/services/ServicesProbe.cs",probe)||!readAsset("tests/fixtures/services/Fase2Probe.cs",fase2)||
     !readAsset("tests/fixtures/audio/astra-audio-probe.wav",wave)) return fail("fixture sources unavailable");
  for(const auto *folder:{"Scripts","Audio","scenes"}){fs::create_directories(root/folder,ec);if(ec) return fail("cannot create folders");}
  if(!editor::EditorImportTransaction::write(root/"Scripts/ServicesProbe.cs",probe)||!editor::EditorImportTransaction::write(root/"Scripts/Fase2Probe.cs",fase2)||
     !editor::EditorImportTransaction::write(root/"Audio/astra-audio-probe.wav",wave)) return fail("cannot publish sources");
  editor::EditorSession session;
  if(!session.setProjectDirectory(root.generic_string().c_str())) return fail("project directory rejected");
  resources::AssetGuid clip;std::string error;
  if(!session.importWaveClip("Audio/astra-audio-probe.wav",clip,error)) return fail(error);
  auto &document=session.document();
  const auto entity=[&](const char *name,float x,float y,float z){
    const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto v=*document.find(id);
    v.transform.position[0]=x;v.transform.position[1]=y;v.transform.position[2]=z;document.applyEntityValues(id,v);return id;};
  // Câmera autorada: o raio do centro da vista atravessa o alvo.
  const auto cameraId=entity("Câmera",0,1,-6);auto camera=*document.find(cameraId);
  if(!camera.components.add(scene::Camera::descriptor)||!document.applyEntityValues(cameraId,camera)) return fail("camera");
  const auto targetId=entity("Alvo",0,1,0);auto target=*document.find(targetId);
  static_cast<scene::PhysicsBody*>(target.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
  auto *targetShape=static_cast<scene::Collider*>(target.components.add(scene::Collider::descriptor));
  targetShape->halfX=targetShape->halfY=targetShape->halfZ=.75f;
  if(!document.applyEntityValues(targetId,target)) return fail("target");
  // Sensor com o Gatilho sonoro e a sonda.
  const auto sensorId=entity("Gatilho sonoro",4,0,0);auto sensor=*document.find(sensorId);
  auto *body=static_cast<scene::PhysicsBody*>(sensor.components.add(scene::PhysicsBody::descriptor));
  body->motion=scene::BodyMotion::Static;body->sensor=true;
  auto *shape=static_cast<scene::Collider*>(sensor.components.add(scene::Collider::descriptor));shape->halfX=shape->halfY=shape->halfZ=1;
  auto *audio=static_cast<scene::AudioSource*>(sensor.components.add(scene::AudioSource::descriptor));
  audio->clip=clip;audio->dimension=scene::AudioDimension::Flat;audio->playback=scene::AudioPlayback::Stopped;audio->volume=1;audio->loop=true;
  auto *connection=static_cast<scene::EventConnection*>(sensor.components.add(scene::EventConnection::descriptor));
  connection->event=3;connection->action=scene::kEventConnectionCallMethod;connection->method=1;
  auto *script=static_cast<scene::ScriptBehavior*>(sensor.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="acceptance.services";script->source="Scripts/ServicesProbe.cs";
  if(!session.history().applyValues(document,sensorId,sensor)) return fail("sound trigger");
  const auto fallingId=entity("Corpo que cai",4,4,0);auto falling=*document.find(fallingId);
  static_cast<scene::PhysicsBody*>(falling.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Dynamic;
  auto *ball=static_cast<scene::Collider*>(falling.components.add(scene::Collider::descriptor));ball->shape=scene::ColliderShape::Sphere;ball->radius=.4f;
  if(!session.history().applyValues(document,fallingId,falling)) return fail("falling body");
  const auto listenerId=entity("Ouvinte",0,1,-6);auto listener=*document.find(listenerId);
  if(!listener.components.add(scene::AudioListener::descriptor)||!document.applyEntityValues(listenerId,listener)) return fail("listener");
  // Fase2: só a sonda que confirma a cena ativa e uma câmera.
  editor::EditorDocument second;second.setTags(document.tags());
  const auto marker=second.createEntity(second.root(),runtime::ObjectKind::Folder,"Fase2");auto value=*second.find(marker);
  value.components.add(scene::Camera::descriptor);
  auto *secondScript=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
  secondScript->scriptType="acceptance.services.fase2";secondScript->source="Scripts/Fase2Probe.cs";
  if(!second.applyEntityValues(marker,value)) return fail("second scene");
  if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",editor::serializeEditorDocument(document,0))||
     !editor::EditorImportTransaction::writeText(root/"scenes/Fase2.aescene",editor::serializeEditorDocument(second,0))) return fail("scenes");
  std::ostringstream descriptor;
  descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"Servicos-20261006\",\"path\":"<<std::quoted(root.generic_string())
            <<",\"template\":\"empty\",\"scenes\":2,\"assets\":1},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
  if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str())) return fail("descriptor");
  std::printf("Services project written: %s clip=%s\n",root.generic_string().c_str(),clip.text().c_str());
  return 0;
}

// Fixture exporter, separate from rendering and never an editor product command.
int writeRuntimeFamilyProject(const char*directory){
 namespace fs=std::filesystem;
 const auto fail=[](const std::string&message){std::fprintf(stderr,"Families export refused: %s\n",message.c_str());return 2;};
 if(!directory||!directory[0])return fail("provide a new empty output directory");
 std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
 if(ec)return fail("invalid output path");
 if(fs::is_symlink(fs::symlink_status(root,ec)))return fail("output directory cannot be a symbolic link");
 ec.clear();
 if(fs::exists(root,ec)&&(!fs::is_directory(root,ec)||!fs::is_empty(root,ec)))return fail("output already contains files; use a new directory");
 if(ec)return fail("cannot inspect output directory");
 std::vector<u8>physicsSource,audioSource,wave;
 if(!readAsset("tests/fixtures/physics2d/Physics2DProbe.cs",physicsSource)||!readAsset("tests/fixtures/audio/AudioProbe.cs",audioSource)||!readAsset("tests/fixtures/audio/astra-audio-probe.wav",wave))return fail("required current probe sources or original WAV unavailable");
 for(const auto*folder:{"Scripts","Audio","scenes"}){fs::create_directories(root/folder,ec);if(ec)return fail("cannot create fixture folders; partial output retained");}
 if(!editor::EditorImportTransaction::write(root/"Scripts/Physics2DProbe.cs",physicsSource)||!editor::EditorImportTransaction::write(root/"Scripts/AudioProbe.cs",audioSource)||!editor::EditorImportTransaction::write(root/"Audio/astra-audio-probe.wav",wave))return fail("cannot copy original fixture bytes; partial output retained");
 editor::EditorSession session;
 if(!session.setProjectDirectory(root.generic_string().c_str()))return fail("EditorSession refused project directory");
 resources::AssetGuid clip;std::string error;
 if(!session.importWaveClip("Audio/astra-audio-probe.wav",clip,error))return fail(error);
 auto&document=session.document();
 const auto bodyId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"PHY2D Body");
 auto body=*document.find(bodyId);
 auto*dynamic=static_cast<scene::Body2D*>(body.components.add(scene::Body2D::descriptor));
 auto*box=static_cast<scene::Collider2D*>(body.components.add(scene::Collider2D::descriptor));
 auto*physicsProbe=static_cast<scene::ScriptBehavior*>(body.components.add(scene::ScriptBehavior::descriptor));
 if(!dynamic||!box||!physicsProbe)return fail("could not author Body2D fixture components");
 dynamic->motion=scene::Body2DMotion::Dynamic;dynamic->gravityScale=0;dynamic->mass=1;dynamic->fixedRotation=true;box->halfX=.5f;box->halfY=.5f;
 physicsProbe->scriptType="acceptance.physics2d";physicsProbe->source="Scripts/Physics2DProbe.cs";
 if(!session.history().applyValues(document,bodyId,body))return fail("Body2D candidate rejected");
 const auto wallId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"PHY2D Wall");auto wall=*document.find(wallId);wall.transform.position[0]=4;
 auto*staticBody=static_cast<scene::Body2D*>(wall.components.add(scene::Body2D::descriptor));auto*wallBox=static_cast<scene::Collider2D*>(wall.components.add(scene::Collider2D::descriptor));
 if(!staticBody||!wallBox)return fail("could not author Static wall");
 staticBody->motion=scene::Body2DMotion::Static;wallBox->halfX=.5f;wallBox->halfY=2;
 if(!session.history().applyValues(document,wallId,wall))return fail("wall candidate rejected");
 const auto sourceId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"AUDIO Source");auto source=*document.find(sourceId);
 auto*audio=static_cast<scene::AudioSource*>(source.components.add(scene::AudioSource::descriptor));auto*audioProbe=static_cast<scene::ScriptBehavior*>(source.components.add(scene::ScriptBehavior::descriptor));
 if(!audio||!audioProbe)return fail("could not author AudioSource fixture");
 audio->clip=clip;audio->dimension=scene::AudioDimension::Flat;audio->playback=scene::AudioPlayback::Stopped;audio->volume=.5f;audio->pitch=1;audio->loop=false;
 audioProbe->scriptType="acceptance.audio";audioProbe->source="Scripts/AudioProbe.cs";
 if(!session.history().applyValues(document,sourceId,source))return fail("AudioSource candidate rejected");
 const auto listenerId=document.createEntity(document.root(),runtime::ObjectKind::Folder,"AUDIO Listener");auto listener=*document.find(listenerId);
 auto*audioListener=static_cast<scene::AudioListener*>(listener.components.add(scene::AudioListener::descriptor));
 if(!audioListener)return fail("could not author AudioListener fixture");
 audioListener->volume=1;audioListener->enabled=true;
 if(!session.history().applyValues(document,listenerId,listener))return fail("AudioListener candidate rejected");
 // No visual mesh or 3D physics is invented. Spatial authoring remains a UI step.
 const auto scene=editor::serializeEditorDocument(document,0);
 if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",scene))return fail("scene serialization publication failed");
 std::vector<u8>savedScene;editor::EditorDocument reopened;
 if(!editor::EditorImportTransaction::read(root/"scenes/editor.aescene",savedScene)||
    !editor::deserializeEditorDocument(std::string_view(reinterpret_cast<const char*>(savedScene.data()),savedScene.size()),0,reopened)||
    editor::serializeEditorDocument(reopened,0)!=scene)return fail("published scene roundtrip failed; partial output retained");
 std::ostringstream descriptor;
 descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":\"Families-ADB-20260930\",\"path\":"<<std::quoted(root.generic_string())<<",\"template\":\"empty\",\"scenes\":1,\"assets\":1},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
 if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str()))return fail("project descriptor publication failed");
 std::printf("Families project written: %s\nBody=%u Wall=%u Audio=%u AudioClip=%s\n",root.generic_string().c_str(),bodyId,wallId,sourceId,clip.text().c_str());return 0;
}

enum class AcceptanceCase {Time,Input,Groups,TimerConnection,Mouse,Profile,RuntimeCapture,PhysicsConnection,Physics2DConnection,TimerControls,TweenControls,TweenConnection,NumberTweens,CharacterGround,CharacterRebuild,CharacterState,CharacterPlatform,CharacterPlatformCarry};
int writeTimeProject(const char *directory,AcceptanceCase acceptance=AcceptanceCase::Time) {
 const bool includeInput=acceptance==AcceptanceCase::Input,includeGroups=acceptance==AcceptanceCase::Groups,
 includeConnection=acceptance==AcceptanceCase::TimerConnection,includeMouse=acceptance==AcceptanceCase::Mouse,
 includeProfile=acceptance==AcceptanceCase::Profile,includeRuntimeCapture=acceptance==AcceptanceCase::RuntimeCapture;
 namespace fs=std::filesystem;
 const auto fail=[](const std::string &message){std::fprintf(stderr,"Time export refused: %s\n",message.c_str());return 2;};
 if(!directory || !directory[0])return fail("provide a new empty output directory");
 std::error_code ec;const auto root=fs::absolute(editor::EditorImportTransaction::fromUtf8(directory),ec).lexically_normal();
 if(ec)return fail("invalid output path");
 if(fs::is_symlink(fs::symlink_status(root,ec)))return fail("output cannot be a symbolic link");
 ec.clear();
 if(fs::exists(root,ec) && (!fs::is_directory(root,ec) || !fs::is_empty(root,ec)))return fail("output already contains files");
 if(ec)return fail("cannot inspect output");
 std::vector<u8> source;if(!readAsset("tests/fixtures/time/TimeProbe.cs",source))return fail("current TimeProbe source unavailable");
 for(const auto *folder:{"Scripts","scenes"}){fs::create_directories(root/folder,ec);if(ec)return fail("cannot create folders; partial output retained");}
 if(!editor::EditorImportTransaction::write(root/"Scripts/TimeProbe.cs",source))return fail("cannot publish probe");
 editor::EditorSession session;
 if(!session.setProjectDirectory(root.generic_string().c_str()))return fail("project directory rejected");
 auto &document=session.document();const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,"TIME Acceptance");
 auto value=*document.find(id);
 auto *scaled=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
 auto *unscaled=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
 auto *probe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
 if(!scaled || !unscaled || !probe)return fail("cannot author timers and behavior");
 scaled->intervalSeconds=.1f;scaled->repeat=true;scaled->enabled=false;
 unscaled->intervalSeconds=.1f;unscaled->repeat=true;unscaled->enabled=false;unscaled->ignoreTimeScale=true;
 probe->scriptType="acceptance.time";probe->source="Scripts/TimeProbe.cs";
 if(includeInput) {
   std::vector<u8> inputSource;
   if(!readAsset("tests/fixtures/input/InputCaptureProbe.cs",inputSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/InputCaptureProbe.cs",inputSource))return fail("cannot publish input probe");
   auto *inputProbe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
   if(!inputProbe)return fail("cannot author input behavior");
   inputProbe->scriptType="acceptance.input.capture";inputProbe->source="Scripts/InputCaptureProbe.cs";
   auto actions=document.inputActions();auto action=*actions.find("Saltar");
   action.bindings={{runtime::InputSource::Key,62}};
   if(!actions.replace(action.id,action) || !session.history().setInputActions(document,actions))return fail("cannot author input action");
 }
 if(includeMouse) {
   std::vector<u8> mouseSource;
   if(!readAsset("tests/fixtures/input/MouseProbe.cs",mouseSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/MouseProbe.cs",mouseSource))return fail("cannot publish mouse probe");
   auto *mouseProbe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
   if(!mouseProbe)return fail("cannot author mouse behavior");
   mouseProbe->scriptType="acceptance.input.mouse";mouseProbe->source="Scripts/MouseProbe.cs";
   auto actions=document.inputActions();auto jump=*actions.find("Saltar");
   jump.bindings={{runtime::InputSource::MouseButton,0}};
   runtime::InputAction look;look.id="MouseLook";look.kind=runtime::ActionKind::Axis2D;look.deadzone=0;
   look.bindings={{runtime::InputSource::MouseAxis,0,0,0},{runtime::InputSource::MouseAxis,1,0,1}};
   runtime::InputAction wheel;wheel.id="MouseWheel";wheel.kind=runtime::ActionKind::Axis1D;wheel.deadzone=0;
   wheel.bindings={{runtime::InputSource::MouseAxis,3}};
   if(!actions.replace(jump.id,jump) || !actions.add(look) || !actions.add(wheel) ||
      !session.history().setInputActions(document,actions))return fail("cannot author mouse actions");
 }
 if(includeProfile) {
   std::vector<u8> profileSource;
   if(!readAsset("tests/fixtures/input/InputProfileProbe.cs",profileSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/InputProfileProbe.cs",profileSource))return fail("cannot publish profile probe");
   auto *profileProbe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
   if(!profileProbe)return fail("cannot author profile behavior");
   profileProbe->scriptType="acceptance.input.profile";profileProbe->source="Scripts/InputProfileProbe.cs";
 }
 if(includeRuntimeCapture) {
   std::vector<u8> captureSource;
   if(!readAsset("tests/fixtures/input/InputRuntimeCaptureProbe.cs",captureSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/InputRuntimeCaptureProbe.cs",captureSource))return fail("cannot publish runtime capture probe");
   auto *captureProbe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
   if(!captureProbe)return fail("cannot author capture behavior");
   captureProbe->scriptType="acceptance.input.runtime-capture";captureProbe->source="Scripts/InputRuntimeCaptureProbe.cs";
 }
 if(includeGroups) {
   std::vector<u8> groupsSource;
   if(!readAsset("tests/fixtures/groups/GroupsProbe.cs",groupsSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/GroupsProbe.cs",groupsSource))return fail("cannot publish groups probe");
   auto *groupsProbe=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));
   if(!groupsProbe)return fail("cannot author groups behavior");
   groupsProbe->scriptType="acceptance.groups";groupsProbe->source="Scripts/GroupsProbe.cs";
   value.groups.add("guardas");value.groups.add("recebe-dano");
   const auto child=document.createEntity(id,runtime::ObjectKind::Folder,"Guarda filho");
   if(!child)return fail("cannot author child member");
   auto childValue=*document.find(child);childValue.groups.add("guardas");
   if(!session.history().applyValues(document,child,childValue))return fail("cannot author child groups");
 }
 if(!session.history().applyValues(document,id,value))return fail("authored candidate rejected");
 if(includeConnection) {
   std::vector<u8> connectionSource;
   if(!readAsset("tests/fixtures/events/TimerConnectionProbe.cs",connectionSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/TimerConnectionProbe.cs",connectionSource))return fail("cannot publish connection probe");
   const auto emitter=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Timeout emissor");
   const auto receiver=document.createEntity(emitter,runtime::ObjectKind::Folder,"Timeout receptor");
   auto receiverValue=*document.find(receiver);receiverValue.active=false;receiverValue.groups.add("timeout-receiver");
   if(!session.history().applyValues(document,receiver,receiverValue))return fail("cannot author receiver");
   auto emitterValue=*document.find(emitter);
   auto *timer=static_cast<scene::Timer*>(emitterValue.components.add(scene::Timer::descriptor));
   auto *probe=static_cast<scene::ScriptBehavior*>(emitterValue.components.add(scene::ScriptBehavior::descriptor));
   if(!timer || !probe)return fail("cannot author timeout connection");
   timer->intervalSeconds=.15f;timer->repeat=false;timer->ignoreTimeScale=true;timer->elapsedAction=1;timer->elapsedTarget=receiver;
   probe->scriptType="acceptance.timer.connection";probe->source="Scripts/TimerConnectionProbe.cs";
   if(!session.history().applyValues(document,emitter,emitterValue))return fail("cannot publish timeout connection");
 }
 if(acceptance==AcceptanceCase::PhysicsConnection) {
   std::vector<u8> connectionSource;
   if(!readAsset("tests/fixtures/events/PhysicsConnectionProbe.cs",connectionSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/PhysicsConnectionProbe.cs",connectionSource))return fail("cannot publish physics probe");
   const auto receiver=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Luz receptora");
   auto lamp=*document.find(receiver);lamp.active=false;lamp.groups.add("physics-event-receiver");
   lamp.components.add(scene::Light::descriptor);lamp.transform.position[0]=2;
   if(!session.history().applyValues(document,receiver,lamp))return fail("cannot author receiver light");
   const auto sensor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Sensor conectado");
   const auto incoming=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Corpo entrante");
   auto incomingValue=*document.find(incoming);incomingValue.transform.position[1]=3;
   auto *incomingBody=static_cast<scene::PhysicsBody*>(incomingValue.components.add(scene::PhysicsBody::descriptor));
   auto *incomingCollider=static_cast<scene::Collider*>(incomingValue.components.add(scene::Collider::descriptor));
   incomingBody->motion=scene::BodyMotion::Dynamic;incomingCollider->halfX=.5f;incomingCollider->halfY=.5f;incomingCollider->halfZ=.5f;
   if(!session.history().applyValues(document,incoming,incomingValue))return fail("cannot author incoming body");
   auto sensorValue=*document.find(sensor);
   auto *body=static_cast<scene::PhysicsBody*>(sensorValue.components.add(scene::PhysicsBody::descriptor));
   auto *shape=static_cast<scene::Collider*>(sensorValue.components.add(scene::Collider::descriptor));
   auto *connection=static_cast<scene::PhysicsEventConnection3D*>(sensorValue.components.add(scene::PhysicsEventConnection3D::descriptor));
   auto *behavior=static_cast<scene::ScriptBehavior*>(sensorValue.components.add(scene::ScriptBehavior::descriptor));
   body->motion=scene::BodyMotion::Static;body->sensor=true;shape->halfX=8;shape->halfY=.5f;shape->halfZ=8;
   connection->action=1;connection->receiver=receiver;connection->otherFilter=incoming;
   behavior->scriptType="acceptance.physics.connection";behavior->source="Scripts/PhysicsConnectionProbe.cs";
   if(!session.history().applyValues(document,sensor,sensorValue))return fail("cannot author connected sensor");
 }
 if(acceptance==AcceptanceCase::Physics2DConnection) {
   std::vector<u8> connectionSource;
   if(!readAsset("tests/fixtures/events/Physics2DConnectionProbe.cs",connectionSource) ||
      !editor::EditorImportTransaction::write(root/"Scripts/Physics2DConnectionProbe.cs",connectionSource))return fail("cannot publish physics probe");
   const auto receiver=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Luz receptora");
   auto lamp=*document.find(receiver);lamp.active=false;lamp.groups.add("physics2d-event-receiver");
   lamp.components.add(scene::Light::descriptor);lamp.transform.position[0]=2;
   if(!session.history().applyValues(document,receiver,lamp))return fail("cannot author receiver light");
   const auto sensor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Sensor conectado");
   const auto incoming=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Corpo entrante");
   auto incomingValue=*document.find(incoming);incomingValue.transform.position[1]=3;
   auto *incomingBody=static_cast<scene::Body2D*>(incomingValue.components.add(scene::Body2D::descriptor));
   auto *incomingCollider=static_cast<scene::Collider2D*>(incomingValue.components.add(scene::Collider2D::descriptor));
   incomingBody->motion=scene::Body2DMotion::Dynamic;incomingCollider->halfX=.5f;incomingCollider->halfY=.5f;
   if(!session.history().applyValues(document,incoming,incomingValue))return fail("cannot author incoming body");
   auto sensorValue=*document.find(sensor);
   auto *body=static_cast<scene::Body2D*>(sensorValue.components.add(scene::Body2D::descriptor));
   auto *shape=static_cast<scene::Collider2D*>(sensorValue.components.add(scene::Collider2D::descriptor));
   auto *connection=static_cast<scene::PhysicsEventConnection2D*>(sensorValue.components.add(scene::PhysicsEventConnection2D::descriptor));
   auto *behavior=static_cast<scene::ScriptBehavior*>(sensorValue.components.add(scene::ScriptBehavior::descriptor));
   body->motion=scene::Body2DMotion::Static;shape->sensor=true;shape->halfX=8;shape->halfY=.5f;
   connection->action=1;connection->receiver=receiver;connection->otherFilter=incoming;
   behavior->scriptType="acceptance.physics2d.connection";behavior->source="Scripts/Physics2DConnectionProbe.cs";
   if(!session.history().applyValues(document,sensor,sensorValue))return fail("cannot author connected sensor");
 }
 if(acceptance==AcceptanceCase::TimerControls) {
   std::vector<u8> controlSource;if(!readAsset("tests/fixtures/time/TimerControlProbe.cs",controlSource)||!editor::EditorImportTransaction::write(root/"Scripts/TimerControlProbe.cs",controlSource))return fail("cannot publish timer control probe");
   const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Timer manual");auto candidate=*document.find(owner);
   auto *timer=static_cast<scene::Timer*>(candidate.components.add(scene::Timer::descriptor));timer->autoStart=false;timer->repeat=false;timer->ignoreTimeScale=true;timer->intervalSeconds=.1f;
   auto *behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.timer.controls";behavior->source="Scripts/TimerControlProbe.cs";
   if(!session.history().applyValues(document,owner,candidate))return fail("cannot publish manual timer");
 }
 if(acceptance==AcceptanceCase::TweenConnection) {
   std::vector<u8> source;if(!editor::EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"tests/fixtures/events/TweenConnectionProbe.cs",source))return fail("cannot read tween connection fixture");
   if(!editor::EditorImportTransaction::writeText(root/"Scripts/TweenConnectionProbe.cs",std::string(reinterpret_cast<const char*>(source.data()),source.size())))return fail("cannot publish tween connection fixture");
   const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Tween conectado");const auto receiver=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Luz receptora");auto lamp=*document.find(receiver);lamp.active=false;lamp.components.add(scene::Light::descriptor);document.applyEntityValues(receiver,lamp);
   auto candidate=*document.find(owner);auto*tween=static_cast<scene::TransformTween*>(candidate.components.add(scene::TransformTween::descriptor));tween->ignoreTimeScale=true;tween->duration=.1f;tween->destination[1]=2;tween->finishedAction=1;tween->finishedTarget=receiver;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.tween.connection";behavior->source="Scripts/TweenConnectionProbe.cs";
   if(!session.history().applyValues(document,owner,candidate))return fail("cannot publish connected tween");
 }
 if(acceptance==AcceptanceCase::CharacterRebuild) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/physics/CharacterRebuildProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/CharacterRebuildProbe.cs",source))return fail("cannot publish character rebuild probe");
   const auto floor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Piso");auto base=*document.find(floor);base.transform.position[1]=-.5f;auto*shape=static_cast<scene::Collider*>(base.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=20;shape->halfY=.5f;auto*body=static_cast<scene::PhysicsBody*>(base.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;if(!document.applyEntityValues(floor,base))return fail("cannot publish real floor");
   const auto actor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Personagem persistente");auto candidate=*document.find(actor);candidate.transform.position[1]=1;auto*character=static_cast<scene::Character*>(candidate.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=5;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.character.rebuild";behavior->source="Scripts/CharacterRebuildProbe.cs";
   if(!document.applyEntityValues(actor,candidate))return fail("cannot publish character rebuild fixture");
 }
 if(acceptance==AcceptanceCase::CharacterState) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/physics/CharacterStateProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/CharacterStateProbe.cs",source))return fail("cannot publish character state probe");
   const auto floor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Piso");auto base=*document.find(floor);base.transform.position[1]=-.5f;auto*shape=static_cast<scene::Collider*>(base.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=20;shape->halfY=.5f;auto*body=static_cast<scene::PhysicsBody*>(base.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;if(!document.applyEntityValues(floor,base))return fail("cannot publish real floor");
   const auto actor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Personagem persistente");auto candidate=*document.find(actor);candidate.transform.position[1]=1;auto*character=static_cast<scene::Character*>(candidate.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=5;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.character.state";behavior->source="Scripts/CharacterStateProbe.cs";
   if(!document.applyEntityValues(actor,candidate))return fail("cannot publish character state fixture");
 }
 if(acceptance==AcceptanceCase::CharacterPlatform) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/physics/CharacterPlatformProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/CharacterPlatformProbe.cs",source))return fail("cannot publish character platform probe");
   const auto floor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Piso");auto base=*document.find(floor);base.transform.position[1]=-.5f;auto*shape=static_cast<scene::Collider*>(base.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=20;shape->halfY=.5f;auto*body=static_cast<scene::PhysicsBody*>(base.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Kinematic;if(!document.applyEntityValues(floor,base))return fail("cannot publish real floor");
   const auto actor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Personagem persistente");auto candidate=*document.find(actor);candidate.transform.position[1]=1;auto*character=static_cast<scene::Character*>(candidate.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=5;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.character.platform";behavior->source="Scripts/CharacterPlatformProbe.cs";
   if(!document.applyEntityValues(actor,candidate))return fail("cannot publish character platform fixture");
 }
 if(acceptance==AcceptanceCase::CharacterPlatformCarry) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/physics/CharacterPlatformCarryProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/CharacterPlatformCarryProbe.cs",source))return fail("cannot publish character platform carry probe");
   const auto floor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Piso");auto base=*document.find(floor);base.transform.position[1]=-.5f;auto*shape=static_cast<scene::Collider*>(base.components.add(scene::Collider::descriptor));shape->halfX=shape->halfZ=20;shape->halfY=.5f;auto*body=static_cast<scene::PhysicsBody*>(base.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Kinematic;if(!document.applyEntityValues(floor,base))return fail("cannot publish real floor");
   const auto actor=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Personagem persistente");auto candidate=*document.find(actor);candidate.transform.position[1]=1;auto*character=static_cast<scene::Character*>(candidate.components.add(scene::Character::descriptor));character->speed=2;character->jumpSpeed=8;character->inheritPlatformHorizontal=true;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.character.platform.carry";behavior->source="Scripts/CharacterPlatformCarryProbe.cs";
   if(!document.applyEntityValues(actor,candidate))return fail("cannot publish character platform carry fixture");
 }
 if(acceptance==AcceptanceCase::CharacterGround) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/physics/CharacterGroundProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/CharacterGroundProbe.cs",source))return fail("cannot publish character ground probe");
   const auto box=[&](const char*name,float x,float y,float hx,float hy,float hz){const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,name);auto value=*document.find(id);value.transform.position[0]=x;value.transform.position[1]=y;auto*shape=static_cast<scene::Collider*>(value.components.add(scene::Collider::descriptor));shape->halfX=hx;shape->halfY=hy;shape->halfZ=hz;auto*body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;return document.applyEntityValues(id,value);};
   if(!box("Piso",0,-.5f,20,.5f,20)||!box("Degrau 30cm",3,.15f,1.5f,.15f,10))return fail("cannot publish physical stairs");
   for(u32 index=0;index<2;++index){const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,index?"Degrau ligado":"Degrau desligado");auto candidate=*document.find(owner);candidate.transform.position[1]=3;candidate.transform.position[2]=index?2.f:-2.f;
     auto*character=static_cast<scene::Character*>(candidate.components.add(scene::Character::descriptor));character->radius=.3f;character->halfHeight=.5f;character->eyeHeight=1.4f;character->speed=2;character->stepHeight=index?.4f:0;character->floorSnapLength=.5f;
     auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.character.ground";behavior->source="Scripts/CharacterGroundProbe.cs";
     if(!document.applyEntityValues(owner,candidate))return fail("cannot publish character");
   }
 }
 if(acceptance==AcceptanceCase::NumberTweens) {
   std::vector<u8> source;if(!readAsset("tests/fixtures/time/NumberTweenProbe.cs",source)||!editor::EditorImportTransaction::write(root/"Scripts/NumberTweenProbe.cs",source))return fail("cannot publish numeric tween probe");
   const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Luz interpolada");auto candidate=*document.find(owner);
   auto*light=static_cast<scene::Light*>(candidate.components.add(scene::Light::descriptor));light->intensity=0;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.number.tween";behavior->source="Scripts/NumberTweenProbe.cs";
   if(!session.history().applyValues(document,owner,candidate))return fail("cannot publish numeric tween light");
 }
 if(acceptance==AcceptanceCase::TweenControls) {
   std::vector<u8> source;if(!editor::EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"tests/fixtures/time/TweenControlProbe.cs",source))return fail("cannot read tween fixture");
   if(!editor::EditorImportTransaction::writeText(root/"Scripts/TweenControlProbe.cs",std::string(reinterpret_cast<const char*>(source.data()),source.size())))return fail("cannot publish tween fixture");
   const auto owner=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Tween manual");auto candidate=*document.find(owner);
   auto*tween=static_cast<scene::TransformTween*>(candidate.components.add(scene::TransformTween::descriptor));tween->autoplay=false;tween->ignoreTimeScale=true;tween->duration=.5f;tween->destination[0]=4;
   auto*behavior=static_cast<scene::ScriptBehavior*>(candidate.components.add(scene::ScriptBehavior::descriptor));behavior->scriptType="acceptance.tween.controls";behavior->source="Scripts/TweenControlProbe.cs";
   if(!session.history().applyValues(document,owner,candidate))return fail("cannot publish manual tween");
 }
 const auto scene=editor::serializeEditorDocument(document,0);
 if(!editor::EditorImportTransaction::writeText(root/"scenes/editor.aescene",scene))return fail("cannot publish scene");
 std::vector<u8> saved;editor::EditorDocument reopened;
 if(!editor::EditorImportTransaction::read(root/"scenes/editor.aescene",saved) ||
    !editor::deserializeEditorDocument(std::string_view(reinterpret_cast<const char*>(saved.data()),saved.size()),0,reopened) ||
    editor::serializeEditorDocument(reopened,0)!=scene)return fail("published scene roundtrip failed");
 std::ostringstream descriptor;
 descriptor<<"{\n  \"format\":\"ASTRA-PROJECT-1\",\n  \"resourceSource\":\"independent\",\n  \"project\":{\"name\":"<<std::quoted(acceptance==AcceptanceCase::CharacterPlatformCarry?"CharacterPlatformCarry-20261001":acceptance==AcceptanceCase::CharacterPlatform?"CharacterPlatform-20261001":acceptance==AcceptanceCase::CharacterState?"CharacterState-20261001":acceptance==AcceptanceCase::CharacterRebuild?"CharacterRebuild-20261001":acceptance==AcceptanceCase::CharacterGround?"CharacterGround-20261001":acceptance==AcceptanceCase::NumberTweens?"NumberTweens-20261001":acceptance==AcceptanceCase::TweenConnection?"TweenConnection-20261001":acceptance==AcceptanceCase::TweenControls?"TweenControls-20261001":acceptance==AcceptanceCase::TimerControls?"TimerControls-20261001":acceptance==AcceptanceCase::Physics2DConnection?"Physics2DConnection-20261001":acceptance==AcceptanceCase::PhysicsConnection?"PhysicsConnection-20261001":includeRuntimeCapture?"RuntimeCapture-20261001":includeProfile?"InputProfile-20261001":includeMouse?"Mouse-20261001":includeConnection?"TimerConnection-20261001":includeGroups?"Groups-20261001":includeInput?"InputCapture-20261001":"Time-20260930")<<",\"path\":"<<std::quoted(root.generic_string())<<",\"template\":\"empty\",\"scenes\":1,\"assets\":0},\n  \"mainScene\":\"scenes/editor.aescene\",\n  \"editorScene\":\"scenes/editor.aescene\"\n}\n";
 if(!editor::EditorImportTransaction::writeText(root/"project.json",descriptor.str()))return fail("cannot publish project descriptor");
 std::printf("Time project written: %s\nObject=%u Scaled=%llu Unscaled=%llu\n",root.generic_string().c_str(),id,
    static_cast<unsigned long long>(scaled->instanceId()),static_cast<unsigned long long>(unscaled->instanceId()));return 0;
}

} // namespace

int main(int argc, char **argv) {
  if(argc>1&&std::string_view(argv[1])=="write-character-platform-carry-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::CharacterPlatformCarry);
  if(argc>1&&std::string_view(argv[1])=="write-character-platform-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::CharacterPlatform);
  if(argc>1&&std::string_view(argv[1])=="write-character-state-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::CharacterState);
  if(argc>1&&std::string_view(argv[1])=="write-character-rebuild-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::CharacterRebuild);
  if(argc>1&&std::string_view(argv[1])=="write-character-ground-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::CharacterGround);
  if(argc>1&&std::string_view(argv[1])=="write-number-tweens-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::NumberTweens);
  if(argc>1&&std::string_view(argv[1])=="write-tween-connection-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::TweenConnection);
  if(argc>1&&std::string_view(argv[1])=="write-tween-controls-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::TweenControls);
  if(argc>1&&std::string_view(argv[1])=="write-timer-controls-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::TimerControls);
  if(argc>1&&std::string_view(argv[1])=="write-physics2d-connection-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::Physics2DConnection);
  if(argc>1&&std::string_view(argv[1])=="write-physics-connection-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::PhysicsConnection);
  if(argc>1&&std::string_view(argv[1])=="write-runtime-capture-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::RuntimeCapture);
  if(argc>1&&std::string_view(argv[1])=="write-input-profile-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::Profile);
  if(argc>1&&std::string_view(argv[1])=="write-mouse-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::Mouse);
  if(argc>1&&std::string_view(argv[1])=="write-time-project")return writeTimeProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-input-capture-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::Input);
  if(argc>1&&std::string_view(argv[1])=="write-timer-connection-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::TimerConnection);
  if(argc>1&&std::string_view(argv[1])=="write-groups-project")return writeTimeProject(argc>2?argv[2]:nullptr,AcceptanceCase::Groups);
  if(argc>1&&std::string_view(argv[1])=="write-services-project")return writeServicesProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-sequence-project")return writeSequenceProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-physics-f-project")return writePhysicsFProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-virtual-camera-project")return writeVirtualCameraProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-audio-mixer-project")return writeAudioMixerProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-property-tween-project")return writePropertyTweenProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-physics-material-project")return writePhysicsMaterialProject(argc>2?argv[2]:nullptr);
  if(argc>1&&std::string_view(argv[1])=="write-runtime-family-project")return writeRuntimeFamilyProject(argc>2?argv[2]:nullptr);
  const char *output = argc > 1 ? argv[1] : "build/editor-preview.ppm";
  const u32 width = argc > 3 ? static_cast<u32>(std::atoi(argv[2])) : 1600;
  const u32 height = argc > 3 ? static_cast<u32>(std::atoi(argv[3])) : 900;

  std::vector<u8> fontBytes;
  std::vector<u8> iconBytes;
  if (!readAsset("assets/astra-visual/ui/astra-ui-font.aeuf", fontBytes)) return 1;
  if (!readAsset("assets/astra-visual/ui/astra-ui-icons.aeui", iconBytes)) return 1;

  ui::UiFont font;
  ui::UiIconAtlas icons;
  if (!font.load(fontBytes)) {
    std::fprintf(stderr, "fonte recusada\n");
    return 1;
  }
  if (!icons.load(iconBytes)) {
    std::fprintf(stderr, "atlas de icones recusado\n");
    return 1;
  }

  editor::EditorDocument document;
  editor::EditorHistory history;
  editor::EditorMapScene map;
  editor::EditorEntityId selection = editor::kInvalidEntity;
  // No synthetic scene. An optional repository-relative AEMAP uses the same
  // import contract as Android, so every hierarchy row has package geometry.
  if(argc>5) {
    std::vector<u8> bytes;renderer::MapPackageView package;
    if(!readAsset(argv[5],bytes) || !renderer::decodeMapPackage(bytes,package) ||
       !map.import(document,package.draws,package.materials)) {
      std::fprintf(stderr,"pacote de cena recusado\n");return 1;
    }
    const auto children=document.childrenOf(document.root());
    if(!children.empty()) selection=children.front();
  }

  editor::EditorScreenState state{};
  runtime::GameWorld characterPreviewWorld;runtime::ScenePhysics characterPreviewRuntime;
  runtime::GameWorld timerPreviewWorld;runtime::SceneTimers timerPreviewRuntime;
  runtime::GameWorld tweenPreviewWorld;runtime::SceneTweens tweenPreviewRuntime;
  resources::AssetRegistry prefabPreviewAssets;
  editor::EditorConsole console;
  editor::EditorCodeWorkspace code;
  editor::EditorValueLibraries swatches;
  if(argc>4 && std::string(argv[4]).starts_with("recipe-")) {
    namespace fs=std::filesystem;const auto path=fs::temp_directory_path()/("astra-recipe-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(path))return 1;
    struct Cleanup{fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}}cleanup{path};
    editor::EditorSession session;session.initialize(&font,&icons);session.setSurface({0,0,float(width),float(height)},{});
    if(!session.setProjectDirectory(path.string().c_str()))return 1;
    auto &doc=session.document();const auto source=doc.createEntity(doc.root(),editor::EditorEntityKind::Folder,"Emissor original");
    const auto a=doc.createEntity(doc.root(),editor::EditorEntityKind::Folder,"Receptor A"),b=doc.createEntity(doc.root(),editor::EditorEntityKind::Folder,"Receptor B");
    selection=doc.createEntity(doc.root(),editor::EditorEntityKind::Folder,"Novo emissor");auto value=*doc.find(source);
    for(const auto target:{u64(source),u64(a),u64(b)}) {
      auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->elapsedTarget=target;timer->elapsedAction=target==a?2:1;timer->intervalSeconds=target==a?.5f:1.5f;timer->repeat=false;
    }
    if(!doc.applyEntityValues(source,value) || !session.openComponentPresets(source,0))return 1;
    std::string error;if(!session.saveComponentRecipe(source,"Ativação temporizada",error))return 1;
    session.setSelection(selection);if(!session.openComponentPresets(selection,0))return 1;
    const auto tap=[&](u32 widget) {
      session.update();ui::UiInputRouter routes;ui::UiDrawList draw;draw.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));
      editor::buildEditorScreen(session.screen(),ui::defaultTheme(),draw,routes);
      for(float y=2;y<height;y+=4)for(float x=2;x<width;x+=4) {
        const auto hit=routes.route({99,ui::UiPointerPhase::Down,{x,y},0});routes.route({99,ui::UiPointerPhase::Up,{x,y},0});
        if(hit.target==ui::UiPointerTarget::Widget && hit.widgetId==widget) {
          session.handlePointer({77,ui::UiPointerPhase::Down,{x,y},0});session.handlePointer({77,ui::UiPointerPhase::Up,{x,y},0});session.update();return true;
        }
      }
      return false;
    };
    if(!tap(editor::widgetId(editor::EditorWidget::PresetChoiceBase)))return 1;
    if(std::string(argv[4])=="recipe-ready")for(u32 input=0;input<2;++input) {
      if(!tap(editor::widgetId(editor::EditorWidget::PresetInputBase)+input))return 1;
      const auto choices=editor::editorRecipeReferenceChoices(doc,session.screen().presetInputChoices,{});
      const auto target=input?b:a;const auto at=std::find(choices.begin(),choices.end(),target);
      if(at==choices.end())return 1;
      const auto choice=editor::widgetId(editor::EditorWidget::ReferenceChoiceBase)+static_cast<u32>(at-choices.begin());
      if(!tap(choice) && !(tap(editor::widgetId(editor::EditorWidget::ReferenceNext)) && tap(choice)))return 1;
    }
    if(std::string(argv[4])=="recipe-picker" && !tap(editor::widgetId(editor::EditorWidget::PresetInputBase)))return 1;
    state=session.screen();document=doc;history=session.history();prefabPreviewAssets=session.assets();state.assetRegistry=&prefabPreviewAssets;
  }
  if(argc>4 && std::string(argv[4]).starts_with("family-base-")) {
    const auto parent=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Pai transformado");
    editor::EditorTransform pose;pose.scale[0]=2;pose.scale[1]=3;pose.scale[2]=4;pose.rotationDegrees[1]=30;
    if(!document.setTransform(parent,pose))return 1;
    selection=document.createEntity(parent,editor::EditorEntityKind::Folder,"Objeto · pose local");
    auto value=*document.find(selection);value.transform.position[0]=2;value.transform.position[1]=1;value.transform.rotationDegrees[2]=35;value.layer=7;value.groups.add("actors");
    if(!document.applyEntityValues(selection,value))return 1;
    state.componentSelection=selection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.expandedComponent=std::string(argv[4])=="family-base-object"?"astra.object":"astra.transform";
  }
  if(argc>4 && std::string(argv[4]).starts_with("audio-")) {
    const std::string mode=argv[4];namespace fs=std::filesystem;
    const auto path=fs::temp_directory_path()/("astra-audio-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(path)||!fs::create_directory(path/"Audio")) return 1;
    struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
    editor::EditorSession session;if(!session.setProjectDirectory(path.string().c_str())) return 1;
    session.setAudioOutput(runtime::SceneAudio::Output::Offline);
    resources::AssetGuid clip;std::string error;
    if(!writePreviewWave(path/"Audio"/"Test tone.wav")||!session.importWaveClip("Audio/Test tone.wav",clip,error)||!session.loadAudioClip(clip,error)) {
      std::fprintf(stderr,"WAV preview: %s\n",error.c_str());return 1;
    }
    const auto *sourceSchema=scene::findComponentSchema("astra.audio.source"),*listenerSchema=scene::findComponentSchema("astra.audio.listener"),*busSchema=scene::findComponentSchema("astra.audio.bus");
    if(!sourceSchema||!listenerSchema||!busSchema) return 1;
    auto &graph=session.document();
    const auto bus=graph.createEntity(graph.root(),editor::EditorEntityKind::Folder,"Música");
    auto busValue=*graph.find(bus);const auto *busComponent=busValue.components.add(*busSchema->type);if(!busComponent) return 1;
    const auto busInstance=busComponent->instanceId();if(!graph.applyEntityValues(bus,busValue)) return 1;
    const auto listener=graph.createEntity(graph.root(),editor::EditorEntityKind::Folder,"Ouvinte principal");
    auto listenerValue=*graph.find(listener);const auto *listenerComponent=listenerValue.components.add(*listenerSchema->type);if(!listenerComponent) return 1;
    const auto listenerInstance=listenerComponent->instanceId();if(!graph.applyEntityValues(listener,listenerValue)) return 1;
    const auto source=graph.createEntity(graph.root(),editor::EditorEntityKind::Folder,"Fonte sonora");
    auto sourceValue=*graph.find(source);auto *sourceComponent=sourceValue.components.add(*sourceSchema->type);if(!sourceComponent) return 1;
    const auto sourceInstance=sourceComponent->instanceId();
    if(!sourceSchema->type->resourceBindings.front().write(*sourceComponent,0,clip)) return 1;
    if(scene::setComponentProperty(sourceValue.components,sourceSchema->type->id,"bus",scene::ObjectReference{bus},sourceInstance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if((mode=="audio-source-space"||mode=="audio-source-cone")&&scene::setComponentProperty(sourceValue.components,sourceSchema->type->id,"dimension",u32{1},sourceInstance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(!graph.applyEntityValues(source,sourceValue)) return 1;
    selection=mode=="audio-bus"?bus:mode=="audio-listener"?listener:source;
    state.expandedNative=mode=="audio-bus"?busInstance:mode=="audio-listener"?listenerInstance:sourceInstance;
    document=graph;prefabPreviewAssets=session.assets();state.assetRegistry=&prefabPreviewAssets;
    state.componentSelection=selection;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="audio-source-playback") state.componentGroup="Reprodução";
    if(mode=="audio-source-space") state.componentGroup="Espaço";
    if(mode=="audio-source-cone") state.componentGroup="Emissão";
    if(mode=="audio-source-search") state.propertyQuery="volume";
    if(mode=="audio-picker"||mode=="audio-picker-search") {
      state.meshPicker=true;state.resourceInstance=sourceInstance;state.resourceProperty="clip";state.resourceSlot=0;
      if(mode=="audio-picker-search") state.meshQuery="nenhum-clipe-com-este-nome";
    }
    if(mode=="audio-catalog") {state.addingComponent=true;state.componentQuery="Audio";}
  }
  if(argc>4 && std::string(argv[4]).starts_with("number-tween-")) {
    const std::string mode=argv[4];const bool camera=mode=="number-tween-camera";
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,camera?"Lente interpolada":"Luz interpolada");auto value=*document.find(selection);
    auto*component=value.components.add(camera?scene::Camera::descriptor:scene::Light::descriptor);const auto instance=component->instanceId();
    if(!camera)static_cast<scene::Light*>(component)->intensity=0;
    if(!document.applyEntityValues(selection,value))return 1;
    runtime::GameWorld world;runtime::SceneNumberTweens tracks;u64 token=0;
    if(!world.load(document)||tracks.create(world,{world.handle(selection),instance},camera?"vertical_fov":"intensity",camera?100.f:8.f,1,0,false,token)!=runtime::WorldStatus::Ok||!tracks.advance(world,.25,.25))return 1;
    static_cast<runtime::SceneGraph&>(document)=world.graph();state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
    state.componentSelection=selection;state.expandedNative=instance;state.componentGroup=camera?"Lente":"Emissão";
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
  }
  if(argc>4 && (std::string(argv[4]).starts_with("character-ground")||std::string(argv[4]).starts_with("character-state"))) {
    const std::string mode=argv[4];selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Personagem · chão");auto value=*document.find(selection);
    auto*character=static_cast<scene::Character*>(value.components.add(scene::Character::descriptor));character->stepHeight=.3f;character->floorSnapLength=.2f;character->inheritPlatformHorizontal=mode=="character-state-carry";const auto instance=character->instanceId();if(!document.applyEntityValues(selection,value))return 1;
    if(mode.starts_with("character-state")) {
      const auto floor=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Piso");auto floorValue=*document.find(floor);floorValue.transform.position[1]=-.5f;
      auto *body=static_cast<scene::PhysicsBody*>(floorValue.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;
      auto *collider=static_cast<scene::Collider*>(floorValue.components.add(scene::Collider::descriptor));collider->halfX=20;collider->halfY=.5f;collider->halfZ=20;document.applyEntityValues(floor,floorValue);
      if(!characterPreviewWorld.load(document)||!characterPreviewRuntime.start(characterPreviewWorld))return 1;
      for(int i=0;i<120;++i)if(!characterPreviewRuntime.advance(1./60,characterPreviewWorld))return 1;
      if(mode=="character-state-air"){characterPreviewRuntime.jumpCharacter(selection,&characterPreviewWorld);characterPreviewRuntime.advance(1./60,characterPreviewWorld);}
      static_cast<runtime::SceneGraph&>(document)=characterPreviewWorld.graph();state.characterRuntime=&characterPreviewRuntime;state.characterWorld=&characterPreviewWorld;state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
    }
    state.componentSelection=selection;state.expandedNative=instance;state.componentGroup=mode=="character-ground-gravity"?"Locomoção":"Chão";
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
  }
  if(argc>4 && std::string(argv[4]).starts_with("constant-force")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Propulsor físico");
    auto value=*document.find(selection);
    auto *body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));
    body->motion=scene::BodyMotion::Dynamic;
    const auto *schema=scene::findComponentSchema("astra.physics.constant_force");if(!schema) return 1;
    auto *force=value.components.add(*schema->type);if(!force) return 1;
    const auto instance=force->instanceId();
    if(scene::setComponentProperty(value.components,schema->type->id,"force_y",12.f,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.componentSelection=selection;state.expandedNative=instance;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    if(std::string(argv[4])=="constant-force-local-torque") state.componentGroup="Torque local";
  }
  if(argc>4 && std::string(argv[4]).starts_with("spring-")) {
    const std::string mode=argv[4];
    auto source=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Fonte · plataforma");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Seguidor com mola");
    auto value=*document.find(selection);
    const auto *schema=scene::findComponentSchema(mode.starts_with("spring-rotation")?"astra.spring.rotation":mode.starts_with("spring-scale")?"astra.spring.scale":"astra.spring.position");
    auto *c=value.components.add(*schema->type);auto instance=c->instanceId();
    if(mode!="spring-missing"&&scene::setComponentProperty(value.components,schema->type->id,"target",scene::ObjectReference{source},instance)!=scene::ComponentPropertyStatus::Applied)return 1;
    if(!document.applyEntityValues(selection,value))return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    if(mode.ends_with("response"))state.componentGroup="Resposta";
    if(mode.ends_with("offset"))state.componentGroup="Ajustes";
    if(mode=="spring-catalog"){state.addingComponent=true;state.componentQuery="Mola";}
  }
  if(argc>4 && std::string(argv[4]).starts_with("field-")) {
    const std::string mode=argv[4];const scene::ComponentType *type=mode.starts_with("field-wind")?&scene::WindField::descriptor:mode.starts_with("field-drag")?&scene::DragField::descriptor:mode.starts_with("field-radial")?&scene::RadialField::descriptor:&scene::GravityField::descriptor;
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Volume físico local");auto value=*document.find(selection);auto *component=value.components.add(*type);auto instance=component->instanceId();auto *field=static_cast<scene::PhysicsFieldProperties*>(component);field->shape=mode.ends_with("sphere")?1:0;field->falloff=2;
    if(!document.applyEntityValues(selection,value))return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.componentGroup=mode.ends_with("volume")||mode.ends_with("sphere")?"Volume":mode.ends_with("scope")?"Alcance":"Efeito";
  }
  if(argc>4 && std::string(argv[4]).starts_with("field2d-")) {
    const std::string mode=argv[4];const scene::ComponentType *type=mode.starts_with("field2d-wind")?&scene::WindField2D::descriptor:mode.starts_with("field2d-drag")?&scene::DragField2D::descriptor:mode.starts_with("field2d-radial")?&scene::RadialField2D::descriptor:&scene::GravityField2D::descriptor;
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Área física XY");auto value=*document.find(selection);auto *component=value.components.add(*type);auto instance=component->instanceId();auto *field=static_cast<scene::PhysicsFieldProperties*>(component);field->shape=mode.ends_with("circle")?1:0;field->falloff=2;
    if(!document.applyEntityValues(selection,value))return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.componentGroup=mode.ends_with("volume")||mode.ends_with("circle")?"Volume":mode.ends_with("scope")?"Alcance":"Efeito";
  }
  if(argc>4 && std::string(argv[4]).starts_with("body-simulation")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Corpo dinâmico");auto value=*document.find(selection);
    auto *body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->continuousCollision=true;body->freezeRotation[0]=true;
    value.components.add(scene::Collider::descriptor);auto instance=body->instanceId();if(!document.applyEntityValues(selection,value))return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.componentGroup=std::string(argv[4]).ends_with("locks")?"Restrições":"Simulação";
  }
  if(argc>4 && std::string(argv[4]).starts_with("constraint-")) {
    const std::string mode=argv[4];
    const auto source=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Fonte · plataforma móvel");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Objeto restringido");
    auto value=*document.find(selection);
    const std::string_view id=mode.starts_with("constraint-parent")?"astra.constraint.parent":
      mode.starts_with("constraint-look")?"astra.constraint.look_at":mode.starts_with("constraint-aim")?"astra.constraint.aim":"astra.constraint.position";
    const auto *schema=scene::findComponentSchema(id);if(!schema) return 1;
    auto *constraint=value.components.add(*schema->type);if(!constraint) return 1;
    const auto instance=constraint->instanceId();
    if(mode!="constraint-missing" && scene::setComponentProperty(value.components,id,"target",scene::ObjectReference{source},instance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.componentSelection=selection;state.expandedNative=instance;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    if(mode=="constraint-position-adjustments" || mode=="constraint-aim-adjustments") state.componentGroup="Ajustes";
    if(mode=="constraint-parent-rotation") state.componentGroup="Rotação";
    if(mode=="constraint-look-mira") state.componentGroup="Mira";
    if(mode=="constraint-search") state.propertyQuery="axis";
    if(mode=="constraint-catalog") {state.addingComponent=true;state.componentQuery="Constraint";}
  }
  if(argc>4 && std::string(argv[4]).starts_with("path-")) {
    const std::string mode=argv[4];const auto pathEntity=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Curva · trilho de câmera");
    auto value=*document.find(pathEntity);auto *path=static_cast<scene::Path*>(value.components.add(scene::Path::descriptor));if(!path)return 1;
    const u64 pathInstance=path->instanceId();
    const u32 count=mode=="path-limit"?128:mode=="path-empty"?0:5;
    for(u32 n=0;n<count;++n){resources::CurvePoint3D point;point.position={float(n)*.6f,float(std::sin(n*.8f))*.5f,0};point.in={-.25f,0,0};point.out={.25f,0,0};u64 id;if(!path->insertPoint(0,point,id))return 1;}
    const u64 selectedPoint=count?path->curve.points[count>1?1:0].id:0;
    if(mode=="path-orientation"&&count>1)path->curve.points[1].rollDegrees=35;
    if(!document.applyEntityValues(pathEntity,value))return 1;
    selection=pathEntity;state.expandedNative=pathInstance;state.componentSelection=selection;state.pathInstance=pathInstance;state.pathPointId=selectedPoint;
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.pathEditorOpen=mode!="path-summary";state.pathTangents=mode=="path-tangents";state.pathOrientation=mode=="path-orientation";state.pathPointList=mode=="path-list"||mode=="path-limit";
    if(mode=="path-tangents")state.pathStatus="";
    if(mode.starts_with("path-follow")) {
      const auto follower=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Seguir percurso");auto followValue=*document.find(follower);
      auto *follow=static_cast<scene::PathFollow*>(followValue.components.add(scene::PathFollow::descriptor));if(!follow)return 1;
      follow->target=mode=="path-follow-missing"?0:pathEntity;const auto followInstance=follow->instanceId();
      if(!document.applyEntityValues(follower,followValue))return 1;
      selection=follower;state.componentSelection=follower;state.expandedNative=followInstance;state.pathEditorOpen=false;
      if(mode.find("execution")!=std::string::npos)state.componentGroup="Execução";
      if(mode.find("orientation")!=std::string::npos)state.componentGroup="Orientação";
      if(mode.ends_with("page2"))state.propertyPage=1;
      if(mode.ends_with("page3"))state.propertyPage=2;
      if(mode.ends_with("page4"))state.propertyPage=3;
      if(mode.starts_with("path-follow-play")||mode=="path-follow-missing") {
        runtime::GameWorld world;runtime::ScenePaths paths;if(!world.load(document))return 1;paths.advance(world,.5);
        for(const auto &diagnostic:paths.diagnostics())if(diagnostic.object==follower){state.followStatus=runtime::ScenePaths::issueText(diagnostic.issue);state.followStatusWarning=true;}
        double distance=0;if(state.followStatus.empty()&&paths.progress(world,follower,distance))state.followStatus="Distância mundial · "+std::to_string(distance)+" u";
        static_cast<runtime::SceneGraph&>(document)=world.graph();state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
      }
    }
  }
  if(argc>4 && std::string(argv[4]).starts_with("tween-")) {
    const std::string mode=argv[4];
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Animação por destino");
    auto value=*document.find(selection);const auto *schema=scene::findComponentSchema("astra.tween.transform");if(!schema) return 1;
    const auto *component=value.components.add(*schema->type);if(!component) return 1;
    const auto instance=component->instanceId();
    if(scene::setComponentProperty(value.components,schema->type->id,"position_x",5.f,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(mode.starts_with("tween-play")&&scene::setComponentProperty(value.components,schema->type->id,"autoplay",false,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="tween-destination") state.componentGroup="Destino";
    if(mode=="tween-repeat") state.componentGroup="Repetição";
    if(mode=="tween-time") state.componentGroup="Tempo";
    if(mode=="tween-search") state.propertyQuery="position";
    if(mode=="tween-connection"||mode.starts_with("tween-play-connection")) {
      const auto receiver=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Receptor da conclusão");document.setActive(receiver,false);auto receiverValue=*document.find(receiver);receiverValue.transform.position[0]=3;document.applyEntityValues(receiver,receiverValue);
      auto candidate=*document.find(selection);auto*tween=static_cast<scene::TransformTween*>(candidate.components.editInstance(instance));tween->finishedAction=1;tween->finishedTarget=receiver;tween->duration=.1f;document.applyEntityValues(selection,candidate);state.componentGroup="Conexão";
    }
    if(mode.starts_with("tween-play")) {
      auto &world=tweenPreviewWorld;auto &tweens=tweenPreviewRuntime;
      if(!world.load(document)||!tweens.advance(world,0)) return 1;
      if(mode=="tween-play-connection-missing") {
        const auto *tween=static_cast<const scene::TransformTween*>(world.graph().find(selection)->components.findInstance(instance));
        if(!tween||world.destroyObject(world.handle(static_cast<u32>(tween->finishedTarget)))!=runtime::WorldStatus::Ok)return 1;
        world.flush();
      }
      if(!mode.starts_with("tween-play-idle")&&(!tweens.restart(world,selection,instance)||!tweens.advance(world,.25))) return 1;
      if(mode=="tween-play-cancel"&&!tweens.cancel(world,selection,instance)) return 1;
      if(mode=="tween-play-paused"){runtime::SceneTweens::State snapshot;if(tweens.command(world,{world.handle(selection),instance},3,snapshot)!=runtime::WorldStatus::Ok)return 1;}
      const auto *runtimeState=tweens.state(selection,instance);if(!runtimeState) return 1;
      state.tweenRuntime=&tweens;
      static_cast<runtime::SceneGraph &>(document)=world.graph();
      state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
      if(mode.ends_with("page2")) state.propertyPage=1;
    }
  }
  if(argc>4 && std::string(argv[4]).starts_with("physics2d-")) {
    const std::string mode=argv[4];
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Corpo XY");
    auto value=*document.find(selection);
    const auto *body=scene::findComponentSchema("astra.physics2d.body"),*collider=scene::findComponentSchema("astra.physics2d.collider");
    if(!body||!collider) return 1;
    const auto *bodyValue=value.components.add(*body->type);const auto *colliderValue=value.components.add(*collider->type);
    if(!bodyValue||!colliderValue) return 1;
    const bool inspectCollider=mode.starts_with("physics2d-collider");
    auto instance=inspectCollider?colliderValue->instanceId():bodyValue->instanceId();
    if(mode.starts_with("physics2d-joint")||mode=="physics2d-force") {
      const auto id=mode=="physics2d-force"?"astra.physics2d.constant-force":"astra.physics2d.joint";
      const auto *schema=scene::findComponentSchema(id);if(!schema) return 1;
      const auto *component=value.components.add(*schema->type);if(!component) return 1;instance=component->instanceId();
      if(mode=="physics2d-force") {
        if(scene::setComponentProperty(value.components,id,"force_y",12.f,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
      } else {
        const auto connected=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Corpo conectado");
        auto connectedValue=*document.find(connected);connectedValue.components.add(*body->type);connectedValue.components.add(*collider->type);
        if(!document.applyEntityValues(connected,connectedValue)) return 1;
        if(scene::setComponentProperty(value.components,id,"target",scene::ObjectReference{connected},instance)!=scene::ComponentPropertyStatus::Applied) return 1;
        const u32 kind=mode.find("distance")!=std::string::npos?3u:mode.find("prismatic")!=std::string::npos?2u:1u;
        if(scene::setComponentProperty(value.components,id,"kind",kind,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
        if(mode.find("motor")!=std::string::npos&&scene::setComponentProperty(value.components,id,"motor_enabled",true,instance)!=scene::ComponentPropertyStatus::Applied) return 1;
        if(mode.find("anchors")!=std::string::npos) state.componentGroup=schema->type->numbers.front().presentation.group;
        if(mode.find("motor")!=std::string::npos) state.componentGroup="Ajustes";
      }
    }
    if(mode=="physics2d-collider-capsule"&&scene::setComponentProperty(value.components,collider->type->id,"shape",u32{2},instance)!=scene::ComponentPropertyStatus::Applied) return 1;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.componentSelection=selection;state.expandedNative=instance;state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="physics2d-body-movement") state.componentGroup="Movimento";
    if(mode=="physics2d-collider-contact") state.componentGroup="Contato";
    if(mode=="physics2d-search") state.propertyQuery="velocity";
    if(mode=="physics2d-catalog") {state.addingComponent=true;state.componentQuery="2D";}
  }
  if(argc>4 && std::string(argv[4]).starts_with("component-surface")) {
    const auto target=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Jogador");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Camera,"Câmera do jogador");
    auto value=*document.find(selection);
    value.components.add(scene::Camera::descriptor);
    auto *follow=static_cast<scene::CameraFollow *>(value.components.add(scene::CameraFollow::descriptor));
    follow->target=target;const auto instance=follow->instanceId();
    value.components.add(scene::Light::descriptor);
    if(!document.applyEntityValues(selection,value)) return 1;
    state.componentSelection=selection;state.expandedNative=instance;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.inspectorSurface=std::string(argv[4])=="component-surface-inspection"?
      editor::EditorInspectorSurface::Inspection:editor::EditorInspectorSurface::Components;
    if(std::string(argv[4])=="component-surface-scroll") state.componentOverviewScroll=100;
    if(std::string(argv[4])=="component-surface-static") {
      state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
      state.expandedNative=0;state.expandedComponent="astra.object";state.inspectorReferenceFlags=true;
    }
    if(std::string(argv[4]).starts_with("component-surface-lightmap")) {
      auto meshValue=*document.find(selection);
      const auto *render=meshValue.components.add(scene::MeshRenderer::descriptor);
      if(!render||!document.applyEntityValues(selection,meshValue)) return 1;
      state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
      state.expandedNative=render->instanceId();state.meshTab=2;
      if(std::string(argv[4])=="component-surface-lightmap-page2") state.propertyPage=1;
    }
    if(std::string(argv[4])=="component-surface-catalog") {state.addingComponent=true;state.componentPreview=0;}
  }
  if(argc>4 && std::string(argv[4]).starts_with("river")) {
    std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
    if(!renderer::appendWaterAuthoringGeometry(renderer::MapVertexStride,32,vertices,indices,draws,materials) || !map.import(document,draws,materials,false)) return 1;
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Water,"River");
    auto value=*document.find(selection);editMeshRenderer(value)->mesh=3;editor::editWaterRoute(value)->count=3;
    editor::editWaterRoute(value)->points[0].position[2]=-12;editor::editWaterRoute(value)->points[1].position[0]=8;editor::editWaterRoute(value)->points[2].position[2]=12;
    if(!document.applyEntityValues(selection,value)) return 1;
    state.waterTab=std::string(argv[4])=="river-physics"?2:std::string(argv[4])=="river-effects"?3:1;
  }
  if(argc>4 && std::string(argv[4])=="create") state.creationMenu=true;
  // Add Component aberto sobre um objeto vazio: o painel inteiro do catálogo.
  if(argc>4 && std::string(argv[4]).starts_with("add")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Objeto vazio");
    state.componentSelection=selection;state.addingComponent=true;
    if(std::string(argv[4])=="add-search") state.componentQuery="cam";
    // Objeto com Câmera: o catálogo sugere Olhar e Acompanhar alvo, e a prévia
    // de Olhar mostra a composição no painel de detalhe.
    if(std::string(argv[4])=="add-preview") {
      auto value=*document.find(selection);editCamera(value);document.applyEntityValues(selection,value);
      state.componentPreview=editor::editorComponentIndex("astra.camera.look")+1;
    }
  }
  if(argc>4 && std::string(argv[4]).starts_with("create")) state.creationAvailable=editor::creationAlwaysAvailable();
  // Menu do componente e lista de opções de enumeração sobre um colisor.
  if(argc>4 && (std::string(argv[4])=="menu" || std::string(argv[4])=="enum")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Porta");
    auto value=*document.find(selection);
    value.components.add(scene::PhysicsBody::descriptor);
    const auto *collider=value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;
    if(std::string(argv[4])=="menu") state.nativeMenu=collider->instanceId();
    else state.enumPicker=editor::widgetId(editor::EditorWidget::ComponentEnumBase)+1;
  }
  // Play com Hierarquia e Inspector abertos sobre o mundo em execução.
  if(argc>4 && std::string(argv[4]).starts_with("play-inspect")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Poste");
    auto value=*document.find(selection);
    value.components.add(scene::Light::descriptor);
    value.components.add(scene::PhysicsBody::descriptor);
    value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(selection,value);
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Chão");
    state.componentSelection=selection;
    state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;
    state.playEditNote="Alterado em Play · volta ao parar";
    if(std::string(argv[4])=="play-inspect-refused") {
      state.playEditNote="Recusado em Play · Estático · ver console";
      state.playEditRefused=true;
    }
  }
  // Comportamento com campos de componente (Unity: `public Rigidbody alvo;`):
  // um atribuído, um cuja instância sumiu e um número, e o seletor filtrado.
  if(argc>4 && std::string(argv[4]).starts_with("script-component")) {
    code.applyBuildReport("ASTRA_CODE 1 1 0 1 \"project.Seguidor\" \"Seguidor\" \"Scripts/Seguidor.cs\" 3 "
                          "\"alvo\" \"Alvo\" \"component:astra.physics.body\" "
                          "\"camera\" \"Câmera\" \"component:astra.camera\" "
                          "\"velocidade\" \"Velocidade\" \"float\"",code.generation());
    code.publishBuild();
    state.code=&code;
    const auto crate=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Caixa");
    auto crateValue=*document.find(crate);
    const u64 body=crateValue.components.add(scene::PhysicsBody::descriptor)->instanceId();
    crateValue.components.add(scene::Collider::descriptor);
    document.applyEntityValues(crate,crateValue);
    const auto barrel=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Barril");
    auto barrelValue=*document.find(barrel);
    barrelValue.components.add(scene::PhysicsBody::descriptor);
    barrelValue.components.add(scene::Collider::descriptor);
    barrelValue.components.add(scene::Collider::descriptor);
    document.applyEntityValues(barrel,barrelValue);
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Luz do poste");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Jogador");
    auto value=*document.find(selection);
    auto *script=static_cast<scene::ScriptBehavior *>(value.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType="project.Seguidor";script->source="Scripts/Seguidor.cs";
    script->setProperty("alvo","component:astra.physics.body",scene::scriptComponentValue(crate,body));
    script->setProperty("camera","component:astra.camera",scene::scriptComponentValue(crate,77));
    script->setProperty("velocidade","float","4.5");
    const u64 instance=script->instanceId();
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedScript=instance;
    if(std::string(argv[4])=="script-component-picker") {
      state.referenceInstance=instance;state.referenceProperty="alvo";state.referenceScript=true;
      state.referenceScriptType="component:astra.physics.body";
    }
  }
  // Arraste do cabeçalho: a Luz levantada sobre a Malha.
  if(argc>4 && std::string(argv[4])=="reorder") {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Poste");
    auto value=*document.find(selection);
    value.components.add(scene::MeshRenderer::descriptor);
    value.components.add(scene::Light::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;
    state.componentReorder=2;state.componentReorderTarget=1;
    state.componentReorderPoint={static_cast<float>(width)-150.0f,190.0f};
  }
  // Listas em campos de script (Unity Manual/InspectorArray): uma de números e
  // uma de componentes aberta, com um elemento escolhido e um ausente; um campo
  // [HideInInspector] que não aparece.
  if(argc>4 && std::string(argv[4])=="script-array") {
    code.applyBuildReport("ASTRA_CODE 3 1 0 1 \"project.Patrulha\" \"Patrulha\" \"Scripts/Patrulha.cs\" 4 "
                          "\"ativo\" \"Ativa\" \"bool\" 0 "
                          "\"esperas\" \"Esperas\" \"array:float\" 0 "
                          "\"pontos\" \"Pontos\" \"array:component:astra.physics.body\" 0 "
                          "\"semente\" \"Semente\" \"int32\" 1",code.generation());
    code.publishBuild();
    state.code=&code;
    std::vector<std::string> bodies;
    for(const char *name:{"Poste A","Poste B"}) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,name);
      auto value=*document.find(id);
      const u64 body=value.components.add(scene::PhysicsBody::descriptor)->instanceId();
      value.components.add(scene::Collider::descriptor);
      document.applyEntityValues(id,value);
      bodies.push_back(scene::scriptComponentValue(id,body));
    }
    bodies.push_back(scene::scriptComponentValue(bodies.size(),99));
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Guarda");
    auto value=*document.find(selection);
    auto *script=static_cast<scene::ScriptBehavior *>(value.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType="project.Patrulha";script->source="Scripts/Patrulha.cs";
    script->setProperty("ativo","bool","true");
    script->setProperty("esperas","array:float",scene::scriptArrayValue({"1.5","2","4"}));
    script->setProperty("pontos","array:component:astra.physics.body",scene::scriptArrayValue(bodies));
    script->setProperty("semente","int32","7");
    const u64 instance=script->instanceId();
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedScript=instance;
    state.expandedScriptArray="pontos";state.scriptArraySelected=2;
  }
  // Janela de cor com alfa, HDR e uma biblioteca de amostras.
  if(argc>4 && std::string(argv[4]).starts_with("color")) {
    state.colorField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);
    state.colorHue=.58f;state.colorSaturation=.72f;state.colorValue=.86f;state.colorAlpha=.8f;state.colorIntensity=1.5f;
    state.colorHasAlpha=true;state.colorHdr=true;
    state.colorOriginal[0]=.9f;state.colorOriginal[1]=.4f;state.colorOriginal[2]=.1f;state.colorOriginal[3]=1;
    swatches.add("Laranja","1 0.35 0.05 1");swatches.add("Céu","0.2 0.5 1 1");swatches.add("Folha","0.1 0.6 0.15 1");
    swatches.add("Neon","4 0.2 2 1");swatches.add("Cinza","0.2 0.2 0.2 1");
    state.colorLibraries=&swatches;
    if(std::string(argv[4])=="color-hsv") state.colorMode=2;
    if(std::string(argv[4])=="color-swatch") state.colorSwatchMenu=2;
  }
  // Editor de gradiente: pôr do sol com transparência e uma parada escolhida.
  if(argc>4 && std::string(argv[4]).starts_with("gradient")) {
    state.gradientField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);state.gradientType="gradient";
    state.gradientDraft="2 4 0 0.02 0.03 0.2 0.35 0.9 0.25 0.08 0.7 1 0.8 0.3 1 0.9 0.95 1 3 0 1 0.6 0.4 1 0.2";
    state.gradientSelected=2;
    swatches.kind=editor::EditorLibraryKind::Gradient;
    swatches.add("Fogo","0 3 0 1 0 0 0.5 1 0.6 0 1 1 1 0.4 2 0 1 1 1");
    swatches.add("Mar","2 2 0 0 0.1 0.3 1 0.2 0.8 0.9 1 0 1");
    swatches.add("Degraus","1 3 0 1 0 0 0.5 0 1 0 1 0 0 1 1 0 1");
    state.gradientLibraries=&swatches;
    if(std::string(argv[4])=="gradient-alpha") {state.gradientSelectedAlpha=true;state.gradientSelected=2;}
  }
  // Editor de curvas: quatro chaves, a segunda escolhida (e quebrada na variante).
  if(argc>4 && std::string(argv[4]).starts_with("curve")) {
    scene::ScriptCurve curve;
    curve.post=scene::CurveWrapMode::PingPong;
    curve.keys={{0,0},{0.35f,1.2f},{0.7f,0.4f},{1,1}};
    if(std::string(argv[4])=="curve-broken") {
      curve.keys[1].broken=true;curve.keys[1].left=scene::CurveTangentMode::Linear;curve.keys[1].right=scene::CurveTangentMode::Constant;
    }
    curve.updateTangents();
    state.curveField=editor::widgetId(editor::EditorWidget::ScriptFieldBase);state.curveType="curve";
    state.curveDraft=scene::scriptCurveValue(curve);state.curveSelected=2;
    editor::frameCurve(curve,state.curveView);state.curveView[2]+=.6f;
    swatches.kind=editor::EditorLibraryKind::Curve;
    for(const auto &preset:scene::curveFactoryPresets) swatches.add(preset.name,preset.value);
    state.curveLibraries=&swatches;
  }
  // LOD Group aberto: barra dividida com o marcador da vista e um nível escolhido.
  if(argc>4 && std::string(argv[4]).starts_with("lod")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Árvore");
    auto value=*document.find(selection);
    const auto *group=value.components.add(scene::LodGroup::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedNative=group->instanceId();
    state.lodSelected=2;state.lodViewPercent=42;
    state.lodStatus="Na vista: LOD 1 · 42% da tela";
    if(std::string(argv[4])=="lod-menu") state.lodMenu=2;
  }
  // Seletor avançado sobre o campo "Conectar corpo" da junta.
  if(argc>4 && std::string(argv[4]).starts_with("picker")) {
    for(const char *name:{"Porta","Portão","Ponte"}) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,name);
      auto value=*document.find(id);value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
      document.applyEntityValues(id,value);
    }
    document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Enfeite");
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Dobradiça");
    auto value=*document.find(selection);
    value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
    const auto *joint=value.components.add(scene::Joint::descriptor);
    document.applyEntityValues(selection,value);
    state.componentSelection=selection;state.expandedNative=joint->instanceId();
    state.referenceInstance=joint->instanceId();state.referenceProperty="connected_body";
    state.pickerAdvanced=true;
    if(std::string(argv[4])=="picker-grid") state.pickerView=1;
    if(std::string(argv[4])=="picker-table") {state.pickerView=2;state.referenceTypeFilter=false;}
    if(std::string(argv[4])=="picker") state.referenceHighlight=2;
  }
  // Inspector travado num objeto e em modo Debug, com outro objeto selecionado.
  if(argc>4 && std::string(argv[4]).starts_with("inspector-debug")) {
    const auto locked=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Lanterna");
    auto value=*document.find(locked);value.components.add(scene::Light::descriptor);value.components.add(scene::PhysicsBody::descriptor);
    value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(locked,value);
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Outro objeto");
    state.inspectorLocked=locked;state.componentSelection=locked;
    state.inspectorDebug=std::string(argv[4])=="inspector-debug";
    state.pingEntity=locked;state.pingUntil=1;state.uiTime=0;
  }
  // Camadas na vista: duas nomeadas, uma escondida, uma fora da seleção.
  if(argc>4 && std::string(argv[4])=="scene-layers") {
    auto layers=document.layers();layers.setName(3,"Cenário");layers.setName(5,"Interface");layers.setName(8,"Gatilhos");
    document.setLayers(layers);
    for(u32 i=0;i<3;++i) {
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,i?"Árvore":"Casa");
      auto value=*document.find(id);value.layer=3;document.applyEntityValues(id,value);
    }
    const auto trigger=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Zona de porta");
    auto value=*document.find(trigger);value.layer=8;document.applyEntityValues(trigger,value);
    state.sceneLayersPanel=true;state.hiddenLayers=1u<<8;state.unpickableLayers=1u<<3;
  }
  // Perfil de ambiente em Propriedades.
  if(argc>4 && (std::string(argv[4])=="profile-asset" || std::string(argv[4])=="multi-profile")) {
    state.profileInspector=resources::assetGuidFromSeed("preview-profile");
    state.profileInspectorName="Tarde nublada";state.profileInspectorPath="Ambientes/Tarde nublada.environment";
    state.profileInspectorRevision=4;state.profileObjects={1,2};
    state.profileGroups={"Geral","Atmosfera","Neblina","Exposição","Pós","Oclusão ambiente","Luz indireta","HDRI"};
    state.profileGroup=2;
    using Row=editor::EditorScreenState::ProfileRow;
    Row fog;fog.kind=Row::Kind::Boolean;fog.label="Neblina";fog.on=true;
    Row color;color.kind=Row::Kind::Triple;color.label="Cor da neblina";color.color=true;color.rgb[0]=.55f;color.rgb[1]=.6f;color.rgb[2]=.7f;
    color.value="0.55  0.6  0.7";
    Row density;density.kind=Row::Kind::Number;density.label="Densidade";density.value="0.02 1/m";
    Row falloff;falloff.kind=Row::Kind::Number;falloff.label="Decaimento por altura";falloff.value="0.15 1/m";
    Row start;start.kind=Row::Kind::Number;start.label="Início";start.value="4 m";start.editable=false;
    state.profileRows={fog,color,density,falloff,start};
    if(std::string(argv[4])=="multi-profile") {
      state.multiAsset.kind=editor::EditorScreenState::MultiAssetView::Kind::Profiles;
      state.multiAsset.title="2 perfis de ambiente";state.multiAsset.items.resize(2);
      state.profileRows[1].mixed=true;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    }
  }
  // HDRI em Propriedades: a prévia vem de um gradiente de céu escrito no atlas.
  if(argc>4 && (std::string(argv[4])=="hdri-asset" || std::string(argv[4])=="multi-hdri")) {
    state.environmentInspector=resources::assetGuidFromSeed("preview-hdri");
    state.environmentInspectorPath="Ambientes/estudio-noturno.hdr";
    state.environmentSaved.panoramaWidth=1024;state.environmentDraft=state.environmentSaved;state.environmentDraft.specularSamples=256;
    state.environmentDerived={"Panorama 1024×512 · 11 níveis","Reflexão GGX 256×256 · 9 níveis","BRDF 128×128 · 7 MB derivados · irradiância SH9"};
    state.environmentObjects={1,2};state.environmentProfiles=1;state.environmentExposure=.5f;
    state.environmentPreview={0,512,512,256};
    if(std::string(argv[4])=="multi-hdri") {
      state.multiAsset.kind=editor::EditorScreenState::MultiAssetView::Kind::EnvironmentMaps;
      state.multiAsset.title="2 mapas HDRI";state.multiAsset.items.resize(2);state.environmentMixed=5;
      state.environmentPending=2;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    }
  }
  // Material do projeto em Propriedades (a vista é a que a sessão prepara).
  if(argc>4 && (std::string(argv[4])=="material-asset" || std::string(argv[4])=="multi-material" || std::string(argv[4])=="material-uv")) {
    state.materialInspector=resources::assetGuidFromSeed("preview-material");state.materialShared=true;
    auto &view=state.materialSlotView;view.slots=1;view.shared=true;view.name="Madeira envernizada";
    state.materialInspectorPath="Materiais/Madeira envernizada.material";state.materialInspectorRevision=7;
    if(std::string(argv[4])=="multi-material") {
      state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
      state.multiAsset.kind=editor::EditorScreenState::MultiAssetView::Kind::Materials;
      state.multiAsset.items.resize(3);state.materialMixed={"tex0","alpha","num0","uv0.offset0"};
    }
    if(std::string(argv[4])=="material-uv") {
      state.texturePicker=true;state.texturePickerScroll=155;state.textureUvLabel="UV 0";
      state.textureWrapLabel="Repetir";state.textureFilterLabel="Linear";
      const char *labels[]{"Desl. U 0","Desl. V 0","Esc. U 1","Esc. V 1","Rot. 0°"};
      for(u32 i=0;i<5;++i) state.textureUvLabels[i]=labels[i];
      state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    }
    state.materialInspectorSlots=5;state.materialInspectorObjects={1,2,3};
    view.textureNames[0]="madeira-cor.png";view.textureOrigins[0]="do material do projeto";
    view.textureNames[1]="madeira-normal.png";view.textureOrigins[1]="do material do projeto · amostragem própria";
    view.textureNames[2]="Textura da fonte de cada uso";view.textureOrigins[2]="da fonte de cada uso";
    view.textureNames[3]="Sem textura";view.textureOrigins[3]="do material do projeto";
  }
  // Barra de status com mensagem do console e trabalhos; "status-tasks" abre a janela.
  if(argc>4 && std::string(argv[4]).starts_with("status")) {
    editor::EditorConsoleEntry warning;warning.severity=editor::EditorConsoleSeverity::Warning;
    warning.message="Textura sem mipmaps: porta-madeira.png";console.add(warning);
    editor::EditorConsoleEntry error;error.severity=editor::EditorConsoleSeverity::Error;
    error.message="Scripts/Porta.cs(12,5): ';' esperado";console.add(error);
    state.console=&console;state.uiTime=0.4;
    using Task=editor::EditorBackgroundTask;
    state.backgroundTasks={{Task::Kind::ProjectOpen,"Abrindo projeto","Recurso 3 de 8 · Lendo casa.glb",.3f,false},
                           {Task::Kind::ModelImport,"Importando modelo","Lendo e preparando a fonte",-1,true},
                           {Task::Kind::CodeBuild,"Compilando scripts","C# do projeto",-1,false}};
    state.backgroundPanel=std::string(argv[4])=="status-tasks";
  }
  // Layouts: um salvo ativo e outro salvo.
  if(argc>4 && std::string(argv[4])=="layouts") {
    state.layoutsPanel=true;
    editor::EditorLayout review;review.name="Revisão";review.hierarchyWidth=190;review.diagnosticDock=true;
    editor::EditorLayout wide;wide.name="Tablet largo";wide.inspectorWidth=340;wide.filesCollapsed=true;
    state.userLayouts={review,wide};
    state.hierarchyWidth=190;state.diagnosticDockOpen=true;
  }
  // Busca global com resultados dos três provedores.
  if(argc>4 && std::string(argv[4])=="global-search") {
    state.globalSearch=true;state.globalQuery="porta";
    state.globalResults={
      {editor::EditorSearchProvider::Scene,0,"Porta da frente","Cena / Casa · 4 componentes",ui::UiIcon::SceneObject,0},
      {editor::EditorSearchProvider::Scene,0,"Porta dos fundos","Cena / Casa / Cozinha · 3 componentes",ui::UiIcon::SceneObject,0},
      {editor::EditorSearchProvider::Project,0,"PortaAutomatica.cs","Projeto / Scripts/Portas",ui::UiIcon::IdeCode,0},
      {editor::EditorSearchProvider::Project,0,"porta-madeira.png","Projeto / Texturas",ui::UiIcon::AssetsTexture,1},
      {editor::EditorSearchProvider::Create,0,"Porta com dobradiça","Criar · corpo, colisor e junta de dobradiça",ui::UiIcon::SceneObjectAdd,2}};
    state.globalCounts={2,2,1};
  }
  // Histórico de Desfazer com passos aplicados e desfeitos.
  if(argc>4 && std::string(argv[4]).starts_with("undo-history")) {
    state.undoHistory=true;state.undoNewestFirst=std::string(argv[4])!="undo-history-oldest";
    state.undoEntries={"Criar objeto \xC2\xB7 Poste","Transformação \xC2\xB7 Cubo","Adicionar componente \xC2\xB7 Poste",
                       "Intensidade das luzes","Renomear \xC2\xB7 Poste alto","Excluir \xC2\xB7 2 objetos"};
    state.undoApplied=4;
  }
  // Vários recursos: "assets-multi" três texturas com perfil diferente em dois
  // campos e uma pendente; "assets-mixed" tipos diferentes; "assets-other" modelos.
  if(argc>4 && std::string(argv[4]).starts_with("assets-")) {
    using View=editor::EditorScreenState::MultiAssetView;auto &view=state.multiAsset;
    const std::string mode=argv[4];
    const auto item=[](const char *name,const char *detail,ui::UiIcon icon,bool active) {
      View::Item value;value.name=name;value.path=name;value.detail=detail;value.icon=static_cast<u32>(icon);value.active=active;return value;
    };
    if(mode=="assets-multi") {
      view.kind=View::Kind::Textures;view.title="3 texturas";
      view.groups={{"Texturas",3,static_cast<u32>(ui::UiIcon::AssetsTexture)}};
      view.items={item("Muro_BaseColor.png","2048\xC3\x97" "2048  \xC2\xB7  Texturas/Muro",ui::UiIcon::AssetsTexture,false),
                  item("Muro_Normal.png","2048\xC3\x97" "2048  \xC2\xB7  Texturas/Muro",ui::UiIcon::AssetsTexture,false),
                  item("Piso_BaseColor.png","1024\xC3\x97" "1024  \xC2\xB7  Texturas/Piso",ui::UiIcon::AssetsTexture,true)};
      view.fields={"Tipo: \xE2\x80\x94","Tamanho: até 2048 px","Mipmaps: sim","Bordas: sem halo","Anisotropia: da qualidade",
                   "Normal Y: \xE2\x80\x94","Cobertura alfa: desligada","Corte da cobertura: 50%","Streaming de mips: sim","Prioridade: 0"};
      view.mixed=(1u<<0)|(1u<<5);view.pending=1;
    } else if(mode=="assets-mixed") {
      view.kind=View::Kind::Mixed;view.title="4 recursos";
      view.note="Tipos diferentes: só o comum aparece. Toque num tipo para estreitar.";
      view.groups={{"Texturas",2,static_cast<u32>(ui::UiIcon::AssetsTexture)},{"Materiais",1,static_cast<u32>(ui::UiIcon::AssetsMaterial)},
                   {"Modelos",1,static_cast<u32>(ui::UiIcon::AssetsFileMesh)}};
      view.items={item("Muro_BaseColor.png","2048\xC3\x97" "2048  \xC2\xB7  Texturas/Muro",ui::UiIcon::AssetsTexture,false),
                  item("Muro_Normal.png","2048\xC3\x97" "2048  \xC2\xB7  Texturas/Muro",ui::UiIcon::AssetsTexture,false),
                  item("Muro.material","Materiais",ui::UiIcon::AssetsMaterial,true),item("Casa.glb","Modelos",ui::UiIcon::AssetsFileMesh,false)};
    } else {
      view.kind=View::Kind::Other;view.title="2 modelos";
      view.note="Sem edição em conjunto para modelos. Toque num item para abri-lo sozinho.";
      view.groups={{"Modelos",2,static_cast<u32>(ui::UiIcon::AssetsFileMesh)}};
      view.items={item("Casa.glb","Modelos",ui::UiIcon::AssetsFileMesh,true),item("Arvore.glb","Modelos",ui::UiIcon::AssetsFileMesh,false)};
    }
  }
  // Multisseleção: três postes com luz (dois com corpo físico), intensidades
  // diferentes; "multi-light" abre a luz, "multi-set" o menu Definir como.
  if(argc>4 && std::string(argv[4]).starts_with("multi")) {
    std::vector<editor::EditorEntityId> ids;u64 lightInstance=0;
    for(u32 i=0;i<3;++i) {
      const char *names[]{"Poste A","Poste B","Poste C"};
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,names[i]);
      auto value=*document.find(id);auto *light=static_cast<scene::Light *>(value.components.add(scene::Light::descriptor));
      light->intensity=i==2?250.f:100.f;
      if(i<2) value.components.add(scene::PhysicsBody::descriptor);
      document.applyEntityValues(id,value);ids.push_back(id);
      if(i==2) lightInstance=static_cast<const scene::Light *>(document.find(id)->components.find(scene::Light::descriptor))->instanceId();
    }
    selection=ids.back();state.selectionSet=ids;state.multiSelect=true;
    state.multi.count=3;state.multi.hidden=1;state.multi.common={lightInstance};
    state.multi.mixed={"name",editor::multiKey(lightInstance,"intensity"),"t.00"};
    state.componentSelection=selection;
    // Olho e mão: poste B escondido, poste A sem seleção pela vista.
    state.sceneHidden={ids[1]};state.scenePickOff={ids[0]};
    if(std::string(argv[4])!="multi") state.expandedNative=lightInstance;
    if(std::string(argv[4])=="multi-set") {
      state.setValueMenu.key=editor::multiKey(lightInstance,"intensity");state.setValueMenu.label="Intensidade";
      state.setValueMenu.rows={{ids[0],"Poste A  \xC2\xB7  100"},{ids[1],"Poste B  \xC2\xB7  100"},{ids[2],"Poste C  \xC2\xB7  250"}};
    }
  }
  // Inspectors focados: um de objeto, um só do componente de luz; ativo o de objeto.
  if(argc>4 && std::string(argv[4]).starts_with("focused")) {
    const auto lamp=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Lanterna");
    auto value=*document.find(lamp);const auto *light=value.components.add(scene::Light::descriptor);
    value.components.add(scene::PhysicsBody::descriptor);value.components.add(scene::Collider::descriptor);
    document.applyEntityValues(lamp,value);
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Outro objeto");
    state.focusedInspectors.push_back({lamp,0,editor::EditorScreenState::FocusedAsset::None,{},{}});state.focusedInspectors.push_back({lamp,light->instanceId(),editor::EditorScreenState::FocusedAsset::None,{},{}});
    state.focusedActive=std::string(argv[4])=="focused-component"?2:1;
    state.focusedCollapsed=std::string(argv[4])=="focused-collapsed";
    state.focusedMenu=std::string(argv[4])=="focused-menu";
  }
  if(argc>4 && std::string(argv[4]).starts_with("groups")) {
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Sentinela do portão");
    auto value=*document.find(selection);
    if(std::string(argv[4])!="groups-empty") {
      value.groups.add("guardas");value.groups.add("recebe-dano");value.groups.add("objetivo-portao");
      if(std::string(argv[4])=="groups-limit") for(u32 n=3;n<runtime::ObjectGroups::MaximumCount;++n)value.groups.add("grupo-"+std::to_string(n));
      document.applyEntityValues(selection,value);
    }
    state.groupPicker=true;state.groupEntity=selection;
  }
  if(argc>4 && std::string(argv[4]).starts_with("project")) {
    state.workspace=editor::EditorWorkspace::Project;
    state.projectSection=std::string(argv[4])=="project-input"?editor::EditorProjectSection::Input:editor::EditorProjectSection::Layers;
    if(std::string(argv[4]).starts_with("project-input-")) {
      state.projectSection=editor::EditorProjectSection::Input;state.inputTab=1;state.inputActionIndex=2;
      auto map=document.inputActions();auto action=map.actions()[2];action.bindings[0].source=runtime::InputSource::Key;action.bindings[0].code=62;
      if(std::string(argv[4])=="project-input-response") {
        state.inputTab=2;action.interaction=runtime::InputInteraction::Hold;action.duration=.5f;
      }
      if(std::string(argv[4]).starts_with("project-input-mouse")){action.bindings[0].source=runtime::InputSource::MouseButton;action.bindings[0].code=1;}
      if(!map.replace(action.id,action) || !document.setInputActions(map)) return 1;
      if(std::string(argv[4])=="project-input-capturing" || std::string(argv[4])=="project-input-mouse-capturing") {
        state.inputCapturing=true;state.inputCapturePrompt="Pressione uma tecla";
        if(std::string(argv[4])=="project-input-mouse-capturing")state.inputCapturePrompt="Clique um botão do mouse";
        state.inputCaptureFeedback="Aguardando nova pressão";
      }
    }
  }
  if(argc>4 && std::string(argv[4])=="create-physics") {
    state.creationMenu=true;state.creationCategory=3;
    editor::findCreationRecipe("physics.dynamic_sphere",&state.creationSelection);
  }
  if(argc>4 && std::string(argv[4])=="river-diagnostics") {
    state.diagnosticDockOpen=true;state.console=&console;
  }
  if(argc>4 && (std::string(argv[4])=="tag-picker" || std::string(argv[4])=="project-tags")) {
    runtime::ObjectTags tags;tags.add("Player");tags.add("Inimigo");tags.add("Interagível");document.setTags(tags);
    selection=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Porta principal");
    auto value=*document.find(selection);value.tag="Interagível";document.applyEntityValues(selection,value);
    if(std::string(argv[4])=="tag-picker") state.tagPicker=true;
    else {state.workspace=editor::EditorWorkspace::Project;state.projectSection=editor::EditorProjectSection::Tags;}
  }
  if(argc>4 && std::string(argv[4]).starts_with("mechanisms")) {
    ae::test::MechanismsFixture fixture;if(!fixture.create())return 1;document=fixture.document;
    const std::string mode=argv[4];selection=fixture.bodies[mode=="mechanisms-spring"?4:3];
    const auto *joint=document.find(selection)->components.find(scene::Joint::descriptor);
    state.componentSelection=selection;state.expandedNative=joint->instanceId();
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    state.componentGroup=mode=="mechanisms-spring"?"Motor":mode=="mechanisms-frame"?"Eixos":"Translação X";
    if(mode=="mechanisms-focused") {
      state.focusedInspectors.push_back({selection,joint->instanceId(),editor::EditorScreenState::FocusedAsset::None,{}, {}});state.focusedActive=1;
      state.compactPanel=editor::EditorScreenState::CompactPanel::Viewport;
    }
    if(mode=="mechanisms-create") {state.creationMenu=true;state.creationCategory=3;editor::findCreationRecipe("physics.joint_six_dof",&state.creationSelection);}
  }
  if(argc>4 && std::string(argv[4])=="collection-overrides") {
    namespace fs=std::filesystem;
    const auto path=fs::temp_directory_path()/("astra-collection-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(path)) return 1;
    struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
    editor::EditorSession session;if(!session.setProjectDirectory(path.string().c_str())) return 1;
    selection=session.document().createEntity(session.document().root(),editor::EditorEntityKind::Folder,"Sequência de animação");
    auto value=*session.document().find(selection);
    auto *animation=static_cast<scene::Animation*>(value.components.add(scene::Animation::descriptor));
    animation->appendClip();const auto second=animation->appendClip();
    if(!session.document().applyEntityValues(selection,value)) return 1;
    std::string error;if(!session.createPrefab(selection,error).valid()) {std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    value=*session.document().find(selection);
    animation=static_cast<scene::Animation*>(value.components.edit(scene::Animation::descriptor));
    animation->moveClip(second,0);session.document().applyEntityValues(selection,value);
    if(!session.inspectPrefabOverrides(selection,state.prefabOverrides,error)) return 1;
    document=session.document();history=session.history();prefabPreviewAssets=session.assets();state.assetRegistry=&prefabPreviewAssets;
    state.prefabOverridesOpen=true;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
  }
  if(argc>4 && std::string(argv[4]).starts_with("prefab-structure")) {
    namespace fs=std::filesystem;
    const auto path=fs::temp_directory_path()/("astra-prefab-structure-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(path))return 1;
    struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}}cleanup{path};
    editor::EditorSession session;if(!session.setProjectDirectory(path.string().c_str()))return 1;
    selection=session.document().createEntity(session.document().root(),editor::EditorEntityKind::Folder,"Receptor temporizado");
    auto value=*session.document().find(selection);value.components.add(scene::Timer::descriptor);
    const auto order=value.components.add(scene::Path::descriptor)->instanceId();session.document().applyEntityValues(selection,value);
    std::string error;const auto asset=session.createPrefab(selection,error);if(!asset.valid())return 1;
    if(!session.instantiatePrefab(asset,session.document().root(),error))return 1;
    value=*session.document().find(selection);u64 added=0;
    if(std::string(argv[4])=="prefab-structure-collection") {
      auto *path=static_cast<scene::Path*>(value.components.editInstance(order));resources::CurvePoint3D p;u64 point=0;
      if(!path->insertPoint(0,p,point))return 1;
      p.position[0]=5;
      if(!path->insertPoint(0,p,point))return 1;
    } else {
      auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->intervalSeconds=3;
      added=timer->instanceId();value.components.moveInstance(order,0);
    }
    session.document().applyEntityValues(selection,value);
    if(!session.inspectPrefabOverrides(selection,state.prefabOverrides,error))return 1;
    state.propertyPage=std::numeric_limits<u32>::max();
    for(u32 i=0;i<state.prefabOverrides.rows.size();++i) {
      const auto &row=state.prefabOverrides.rows[i];
      if((std::string(argv[4])=="prefab-structure-add" && row.component==added) ||
         (std::string(argv[4])=="prefab-structure-order" && row.kind==editor::PrefabOverrideKind::ComponentOrder) ||
         (std::string(argv[4])=="prefab-structure-collection" && row.component==order))state.propertyPage=i;
    }
    document=session.document();history=session.history();prefabPreviewAssets=session.assets();state.assetRegistry=&prefabPreviewAssets;
    state.prefabOverridesOpen=true;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
  }
  if(argc>4 && std::string(argv[4]).starts_with("prefab-") && !std::string(argv[4]).starts_with("prefab-structure")) {
    // Exercise the actual source/instance comparison, not fabricated UI rows.
    namespace fs=std::filesystem;
    const auto path=fs::temp_directory_path()/("astra-prefab-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(path)) return 1;
    struct Cleanup {fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
    editor::EditorSession session;
    if(!session.setProjectDirectory(path.string().c_str())) return 1;
    selection=session.document().createEntity(session.document().root(),editor::EditorEntityKind::Folder,"Câmera de acompanhamento");
    auto value=*session.document().find(selection);
    auto *follow=static_cast<scene::CameraFollow*>(value.components.add(scene::CameraFollow::descriptor));
    follow->dampingSeconds=.2f;session.document().applyEntityValues(selection,value);
    std::string error;const auto prefabAsset=session.createPrefab(selection,error);if(!prefabAsset.valid()) {std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    value=*session.document().find(selection);
    follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
    follow->dampingSeconds=2;follow->offset[0]=9;value.visible=false;
    session.document().applyEntityValues(selection,value);
    if(std::string(argv[4]).starts_with("prefab-merge")) {
      runtime::Prefab source;if(!session.loadPrefab(prefabAsset,source,error)) return 1;
      auto graph=source.graph();auto value=*graph.find(selection);
      auto *follow=static_cast<scene::CameraFollow*>(value.components.edit(scene::CameraFollow::descriptor));
      follow->dampingSeconds=.6f;follow->offset[1]=6;graph.applyEntityValues(selection,value);
      if(!source.capture(graph,selection,prefabAsset,error) ||
         !editor::EditorImportTransaction::writeText(path/editor::EditorImportTransaction::fromUtf8(session.assets().find(prefabAsset)->path),source.write())) return 1;
    }
    if(!session.inspectPrefabOverrides(selection,state.prefabOverrides,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    document=session.document();history=session.history();state.prefabOverridesOpen=std::string(argv[4])!="prefab-inspector";
    prefabPreviewAssets=session.assets();state.assetRegistry=&prefabPreviewAssets;
    if(!state.prefabOverridesOpen) {
      state.componentSelection=selection;
      state.expandedNative=value.components.find(scene::CameraFollow::descriptor)->instanceId();
    }
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(std::string(argv[4])=="prefab-overrides-stale") document.setName(selection,"Alterado depois da comparação");
    if(std::string(argv[4]).starts_with("prefab-merge")) {
      if(std::string(argv[4])=="prefab-merge-inherited") {
        for(u32 i=0;i<state.prefabOverrides.rows.size();++i)
          if(state.prefabOverrides.rows[i].origin==editor::PrefabOverrideOrigin::Inherited) {state.propertyPage=i;break;}
      } else state.propertyPage=std::numeric_limits<u32>::max();
    }
  }
  state.surface = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
  state.document = &document;
  state.selection = selection;
  state.projectName = argc>5 ? "Package preview" : "Empty Scene";
  if(argc>4 && std::string(argv[4])=="rotate") state.tool=editor::EditorGizmoMode::Rotate;
  if(argc>4 && std::string(argv[4])=="rename") {
    state.renameEntity=state.selection;
    std::snprintf(state.renameText,sizeof(state.renameText),"Objeto editavel");
  }
  if(argc>4 && std::string(argv[4])=="numeric") {
    state.numericField=editor::transformFieldWidget(0,0);
    std::snprintf(state.numericText,sizeof(state.numericText),"12.5");
  }
  if(argc>4 && std::string(argv[4])=="material") {

    state.tab=editor::EditorInspectorTab::Material;
  }
  if(argc>4 && std::string(argv[4])=="lighting") {
    state.selection=document.root();state.workspace=editor::EditorWorkspace::Lighting;
  }
  if(argc>4 && std::string(argv[4]).starts_with("play-time")) {
    state.workspace=editor::EditorWorkspace::Play;
    state.playTimeScale=std::string(argv[4])=="play-time-zero"?0:.5f;
    state.playPaused=std::string(argv[4])=="play-time-step";
  }
  if(argc>4 && std::string(argv[4]).starts_with("timer-runtime")) {
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Timer da porta");auto value=*document.find(id);
    auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));timer->autoStart=false;timer->intervalSeconds=2;const auto instance=timer->instanceId();document.applyEntityValues(id,value);
    if(!timerPreviewWorld.load(document))return 1;
    runtime::SceneTimers::State snapshot;
    if(timerPreviewRuntime.command(timerPreviewWorld,{timerPreviewWorld.handle(id),instance},1,0,snapshot)!=runtime::WorldStatus::Ok)return 1;
    if(!timerPreviewRuntime.advance(timerPreviewWorld,.2,[](runtime::ObjectId,u64,u32){return true;}))return 1;
    if(std::string(argv[4])=="timer-runtime-paused")timerPreviewRuntime.command(timerPreviewWorld,{timerPreviewWorld.handle(id),instance},3,0,snapshot);
    state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Disparo";state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.workspace=editor::EditorWorkspace::Play;state.playInspect=true;state.timerRuntime=&timerPreviewRuntime;
  }
  if(argc>4 && (std::string(argv[4])=="timer-unscaled" || std::string(argv[4])=="timer-connection")) {
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Timer de interface");
    auto value=*document.find(id);auto *timer=static_cast<scene::Timer*>(value.components.add(scene::Timer::descriptor));
    timer->ignoreTimeScale=true;timer->intervalSeconds=.5f;const auto instance=timer->instanceId();
    const bool connection=std::string(argv[4])=="timer-connection";
    if(connection) {timer->elapsedAction=1;timer->elapsedTarget=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Portão");}
    if(!document.applyEntityValues(id,value)) return 1;
    state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup=connection?"Conexão":"Disparo";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;
    state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
  }
  if(argc>4 && std::string(argv[4]).starts_with("physics-connection")) {
    const auto receiver=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Luz do portão");
    auto lamp=*document.find(receiver);lamp.active=false;lamp.transform.position[0]=2;lamp.components.add(scene::Light::descriptor);document.applyEntityValues(receiver,lamp);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Sensor do portão");auto value=*document.find(id);
    auto *body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;body->sensor=true;
    value.components.add(scene::Collider::descriptor);
    auto *connection=static_cast<scene::PhysicsEventConnection3D*>(value.components.add(scene::PhysicsEventConnection3D::descriptor));connection->action=1;connection->receiver=receiver;const auto instance=connection->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Conexão";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(std::string(argv[4])=="physics-connection-focused") {state.focusedInspectors.push_back({id,instance,editor::EditorScreenState::FocusedAsset::None,{},{}});state.focusedActive=1;}
  }
  if(argc>4 && std::string(argv[4]).starts_with("physics2d-connection")) {
    const auto receiver=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Luz do portão");
    auto lamp=*document.find(receiver);lamp.active=false;lamp.transform.position[0]=2;lamp.components.add(scene::Light::descriptor);document.applyEntityValues(receiver,lamp);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Sensor do portão");auto value=*document.find(id);
    auto *body=static_cast<scene::Body2D*>(value.components.add(scene::Body2D::descriptor));body->motion=scene::Body2DMotion::Static;
    static_cast<scene::Collider2D*>(value.components.add(scene::Collider2D::descriptor))->sensor=true;
    auto *connection=static_cast<scene::PhysicsEventConnection2D*>(value.components.add(scene::PhysicsEventConnection2D::descriptor));connection->action=1;connection->receiver=receiver;const auto instance=connection->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Conexão";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(std::string(argv[4])=="physics2d-connection-focused") {state.focusedInspectors.push_back({id,instance,editor::EditorScreenState::FocusedAsset::None,{},{}});state.focusedActive=1;}
  }
  if(argc>4 && std::string(argv[4]).starts_with("event-connection")) {
    const std::string mode=argv[4];
    const auto door=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Porta automática");
    auto doorValue=*document.find(door);doorValue.transform.position[0]=2.5f;
    static_cast<scene::Timer*>(doorValue.components.add(scene::Timer::descriptor))->autoStart=false;document.applyEntityValues(door,doorValue);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Sensor da entrada");auto value=*document.find(id);
    auto *body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Static;body->sensor=true;
    value.components.add(scene::Collider::descriptor);
    auto *connection=static_cast<scene::EventConnection*>(value.components.add(scene::EventConnection::descriptor));
    connection->event=3;connection->action=scene::kEventConnectionCallMethod;connection->method=6;connection->argument=.5f;connection->receiver=door;
    if(mode=="event-connection-activation"){connection->action=1;connection->method=0;}
    const auto instance=connection->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="event-connection-when")state.componentGroup="Quando";
    if(mode=="event-connection-then"||mode=="event-connection-activation")state.componentGroup="Então";
    if(mode=="event-connection-catalog"){state.addingComponent=true;state.componentQuery="Conex";}
  }
  // Consultas físicas: raio de chão e braço de mola de câmera.
  if(argc>4 && (std::string(argv[4])=="raycast"||std::string(argv[4])=="spring-arm")) {
    const bool rayMode=std::string(argv[4])=="raycast";
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,rayMode?"Sensor de chão":"Braço da câmera");auto value=*document.find(id);
    value.transform.position[1]=1.5f;u64 instance=0;
    if(rayMode){auto *r=static_cast<scene::RayCast*>(value.components.add(scene::RayCast::descriptor));r->target[1]=-2;r->filter.layer=1;instance=r->instanceId();}
    else {auto *a=static_cast<scene::SpringArm*>(value.components.add(scene::SpringArm::descriptor));a->length=4;a->radius=.2f;instance=a->instanceId();}
    if(!document.applyEntityValues(id,value))return 1;
    if(!rayMode) document.createEntity(id,editor::EditorEntityKind::Folder,"Câmera");
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup=rayMode?"Consulta":"Braço";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
  }
  // Junta com limites de quebra (aba Quebra).
  if(argc>4 && std::string(argv[4])=="joint-break") {
    const auto anchor=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Viga");auto a=*document.find(anchor);
    a.transform.position[1]=3;static_cast<scene::PhysicsBody*>(a.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Static;
    a.components.add(scene::Collider::descriptor);document.applyEntityValues(anchor,a);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Lustre");auto value=*document.find(id);
    value.transform.position[1]=2;static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor))->motion=scene::BodyMotion::Dynamic;
    value.components.add(scene::Collider::descriptor);
    auto *joint=static_cast<scene::Joint*>(value.components.add(scene::Joint::descriptor));joint->kind=scene::JointKind::Fixed;joint->connectedBody=anchor;
    joint->breakForce=400;joint->breakTorque=120;const auto instance=joint->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Quebra";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
  }
  // Tween de propriedade: acender a luz do palco; "-picker" abre o seletor.
  if(argc>4 && std::string(argv[4]).starts_with("property-tween")) {
    const std::string mode=argv[4];
    const auto lamp=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Luz do palco");auto lampValue=*document.find(lamp);
    lampValue.transform.position[1]=2;static_cast<scene::Light*>(lampValue.components.add(scene::Light::descriptor))->intensity=200;
    document.applyEntityValues(lamp,lampValue);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Acender palco");auto value=*document.find(id);
    auto *tween=static_cast<scene::PropertyTween*>(value.components.add(scene::PropertyTween::descriptor));
    tween->target=lamp;tween->componentType="astra.render.light";tween->property="intensity";tween->destination=1500;tween->duration=2;tween->easing=1;
    const auto instance=tween->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Propriedade";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="property-tween-picker") state.propertyTweenPicker=instance;
  }
  // Material físico: aba Material do Corpo físico e seletor com "Criar".
  if(argc>4 && std::string(argv[4]).starts_with("physics-material")) {
    const std::string mode=argv[4];
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Bola de borracha");auto value=*document.find(id);
    auto *body=static_cast<scene::PhysicsBody*>(value.components.add(scene::PhysicsBody::descriptor));
    body->motion=scene::BodyMotion::Dynamic;body->friction=.9f;body->restitution=.85f;body->restitutionCombine=4;
    auto *shape=static_cast<scene::Collider*>(value.components.add(scene::Collider::descriptor));shape->shape=scene::ColliderShape::Sphere;shape->radius=.4f;
    const auto instance=body->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;state.componentGroup="Material";
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    if(mode=="physics-material-picker"){state.meshPicker=true;state.resourceInstance=instance;state.resourceProperty="material";state.resourceSlot=0;}
    if(mode=="physics-material-collider") {
      auto values=*document.find(id);auto *own=static_cast<scene::Collider*>(values.components.edit(scene::Collider::descriptor));
      own->ownMaterial=true;own->friction=.05f;own->restitution=.1f;own->surface=8;own->frictionCombine=2;
      document.applyEntityValues(id,values);state.expandedNative=own->instanceId();
    }
    if(mode=="physics-material-asset") {
      state.physicsMaterialInspector=resources::assetGuidFromSeed("borracha");state.physicsMaterialView.guid=state.physicsMaterialInspector;
      state.physicsMaterialView.name="Borracha";state.physicsMaterialView.revision=3;state.physicsMaterialView.friction=.9f;
      state.physicsMaterialView.restitution=.85f;state.physicsMaterialView.restitutionCombine=4;state.physicsMaterialView.surface=9;
      state.physicsMaterialPath="Física/Borracha.physmat";state.physicsMaterialUsers=2;
    }
  }
  // Sequência de tweens: porta abre, depois luz e placa juntas após um intervalo.
  if(argc>4 && std::string(argv[4]).starts_with("tween-sequence")) {
    const std::string mode=argv[4];
    const auto tweened=[&](const char *name,float x){
      const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,name);auto v=*document.find(id);v.transform.position[0]=x;
      auto *t=static_cast<scene::TransformTween*>(v.components.add(scene::TransformTween::descriptor));t->autoplay=false;t->destination[1]=1;
      document.applyEntityValues(id,v);return id;};
    const auto doorId=tweened("Porta",-2),lampId=tweened("Luz do corredor",0),signId=tweened("Placa",2);
    const auto id=document.createEntity(document.root(),editor::EditorEntityKind::Folder,"Abertura da sala");auto value=*document.find(id);
    auto *sequence=static_cast<scene::TweenSequence*>(value.components.add(scene::TweenSequence::descriptor));
    sequence->steps[0]={doorId,false,0};sequence->steps[1]={lampId,false,.3f};sequence->steps[2]={signId,true,0};sequence->loops=1;
    const auto instance=sequence->instanceId();
    if(!document.applyEntityValues(id,value))return 1;
    selection=id;state.selection=id;state.componentSelection=id;state.expandedNative=instance;
    state.inspectorSurface=editor::EditorInspectorSurface::Inspection;state.compactPanel=editor::EditorScreenState::CompactPanel::Inspector;
    state.componentGroup=mode=="tween-sequence-run"?"Execução":"Etapas";
    if(mode=="tween-sequence-catalog"){state.addingComponent=true;state.componentQuery="Sequ";}
  }
  if(argc>4 && std::string(argv[4])=="debug-lines") state.workspace=editor::EditorWorkspace::Play;
  state.canUndo = history.canUndo();
  state.canRedo = history.canRedo();

  // Uma camera de editor olhando o objeto selecionado de cima e de lado. Sem
  // ela nao ha grade nem gizmo, e a previa mostraria a interface sobre o vazio.
  editor::EditorViewport view{};
  const bool pathPreview=argc>4&&std::string(argv[4]).starts_with("path-");
  const bool fieldPreview=argc>4&&std::string(argv[4]).starts_with("field-");
  const bool field2DPreview=argc>4&&std::string(argv[4]).starts_with("field2d-");
  const bool characterPreview=argc>4&&(std::string(argv[4]).starts_with("character-ground")||std::string(argv[4]).starts_with("character-state"));
  const float eye[3] = {field2DPreview||fieldPreview||characterPreview?0.f:pathPreview?1.2f:6.0f,field2DPreview?0.f:fieldPreview?5.f:characterPreview||pathPreview?1.f:4.5f,field2DPreview||fieldPreview?-16.f:characterPreview?-5.f:pathPreview?-4.f:-9.f};
  renderer::PerspectiveVisibilitySettings visibility{};
  view.frustum = renderer::buildPerspectiveFrustum(
      eye, field2DPreview||fieldPreview||characterPreview||pathPreview?0.f:.35f,field2DPreview?0.f:fieldPreview?.3f:characterPreview?0.f:pathPreview?.2f:.42f, static_cast<float>(width) / static_cast<float>(height), visibility);
  state.view = &view;

  ui::UiDrawList list;
  list.begin(state.surface, font.metrics(ui::UiFontWeight::Regular));
  ui::UiInputRouter router;
  router.beginFrame();
  // O retangulo da vista so existe depois do layout, e o layout precisa da vista
  // para desenhar a grade. Duas passagens resolvem: a primeira descobre onde a
  // cena mora, a segunda desenha com a projecao certa.
  editor::EditorScreenLayout layout =
      editor::buildEditorScreen(state, ui::defaultTheme(), list, router);
  view.rect = layout.viewport;
  // Debug.DrawLine/DrawRay como o Play desenha: mesma lista e mesma projeção.
  static runtime::DebugLines debugLines;
  if(argc>4 && std::string(argv[4])=="debug-lines") {
    const float origin[3]{0,0,0},x[3]{2,0,0},y[3]{0,2,0},z[3]{0,0,2},target[3]{3,1.5f,1};
    debugLines.add(origin,x,0xFFE5484D,1);debugLines.add(origin,y,0xFF46C97A,1);debugLines.add(origin,z,0xFF4D8DF7,1);
    debugLines.add(origin,target,0xFFF2C14E,1);
    for(int i=0;i<12;++i){const float a=i*0.5235988f,b=(i+1)*0.5235988f;const float p[3]{std::cos(a)*1.5f,0,std::sin(a)*1.5f},q[3]{std::cos(b)*1.5f,0,std::sin(b)*1.5f};debugLines.add(p,q,0xFFFFFFFF,1);}
    const float debugEye[3]{1.5f,2.5f,-6.f};
    view.frustum=renderer::buildPerspectiveFrustum(debugEye,-.077f,.298f,layout.viewport.width/layout.viewport.height,visibility);
    state.debugLines=&debugLines;state.debugView=view;
  }
  if(pathPreview||characterPreview||fieldPreview||field2DPreview)view.frustum=renderer::buildPerspectiveFrustum(eye,0,field2DPreview?0.f:fieldPreview?.3f:characterPreview?0.f:.2f,layout.viewport.width/layout.viewport.height,visibility);
  list.begin(state.surface, font.metrics(ui::UiFontWeight::Regular));
  router.beginFrame();
  layout = editor::buildEditorScreen(state, ui::defaultTheme(), list, router);

  std::vector<ui::UiInstance> instances;
  const ui::UiInstanceBuildResult built =
      ui::buildUiInstances(list, font, icons, 16384, instances);

  test::UiSoftwareTarget target;
  // Um cinza-azulado no lugar da cena 3D: preto esconderia um painel preto que
  // não foi desenhado, e é justamente isso que a pré-visualização tem de expor.
  target.resize(width, height, 0.16f, 0.18f, 0.20f);
  test::rasterizeUi(instances, font, icons, target);

  if (!writePpm(output, target)) {
    std::fprintf(stderr, "nao escreveu %s\n", output);
    return 1;
  }
  std::printf("%s %ux%u\n", output, width, height);
  std::printf("comandos=%u instancias=%u descartadas=%u sem_fonte=%u recortados=%u\n",
              list.commandCount(), built.emitted, built.dropped, built.missingGlyphRuns,
              list.culledCommandCount());
  std::printf("hierarquia=%u linhas viewport=%.0fx%.0f\n", layout.hierarchyRowCount,
              static_cast<double>(layout.viewport.width),
              static_cast<double>(layout.viewport.height));
  return 0;
}
