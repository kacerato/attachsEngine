#include "resources/unity_prefab_import.h"
#include "core/rotation_math.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ae::resources {
namespace {
using Node=UnityYamlNode;
bool fail(std::string &error,const std::string &message){error=message;return false;}
bool ref(const Node &node,std::string_view key,UnityObjectReference &out) {const auto *value=node.get(key);return value&&unityObjectReference(*value,out);}
bool flag(const Node &node,std::string_view key,bool &out,bool optional=false) {const auto *value=node.get(key);return value?value->boolean(out):optional;}
template<usize N> bool vector(const Node &node,std::string_view key,std::array<float,N> &out) {
  const auto *value=node.get(key);if(!value)return false;
  constexpr const char *names[]{"x","y","z","w"};
  for(usize c=0;c<N;++c){double n=0;const auto *part=value->get(names[c]);if(!part||!part->number(n)||std::abs(n)>1e7)return false;out[c]=static_cast<float>(n);}
  return true;
}
// Leitura tolerante dos componentes do G6a: campo ausente fica no padrão da
// Unity; valor malformado recusa o prefab (dado corrompido, não perda).
float num(const Node &node,std::string_view key,float fallback) {
  double value=0;const auto *found=node.get(key);
  return found&&found->number(value)&&std::abs(value)<=1e7?static_cast<float>(value):fallback;
}
i64 integer(const Node &node,std::string_view key,i64 fallback) {i64 value=0;const auto *found=node.get(key);return found&&found->integer(value)?value:fallback;}
bool boolean(const Node &node,std::string_view key,bool fallback) {bool value=false;const auto *found=node.get(key);return found&&found->boolean(value)?value:fallback;}
void vec3(const Node &node,std::string_view key,float (&out)[3]) {
  std::array<float,3> value{};if(vector(node,key,value))std::copy(value.begin(),value.end(),out);
}
// Euler da Unity (graus, ordem Z X Y) refletido em Z, como euler XYZ da Astra.
void reflectedEuler(const float (&unity)[3],float (&astra)[3]) {
  const float x=unity[0]*.00872664626f,y=unity[1]*.00872664626f,z=unity[2]*.00872664626f;
  const float qx[4]{std::sin(x),0,0,std::cos(x)},qy[4]{0,std::sin(y),0,std::cos(y)},qz[4]{0,0,std::sin(z),std::cos(z)};
  const auto mul=[](const float (&a)[4],const float (&b)[4],float (&o)[4]) {
    const float r[4]{a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
                     a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
    std::copy(r,r+4,o);
  };
  float t[4],q[4];mul(qx,qz,t);mul(qy,t,q);q[0]=-q[0];q[1]=-q[1];
  if(!(unity[0]||unity[1]||unity[2])){astra[0]=astra[1]=astra[2]=0;return;}
  rotationEulerXYZ(q,astra);
}
void note(std::vector<std::string> &list,std::string text) {
  if(std::find(list.begin(),list.end(),text)==list.end())list.push_back(std::move(text));
}
// Primeira e última chave de uma curva da Unity, vezes o multiplicador.
void curveEnds(const Node *curve,float multiplier,float &first,float &last) {
  first=last=multiplier;
  const auto *keys=curve?curve->get("m_Curve"):nullptr;
  if(!keys||keys->kind!=Node::Kind::Sequence||keys->elements.empty())return;
  first=num(keys->elements.front(),"value",1)*multiplier;last=num(keys->elements.back(),"value",1)*multiplier;
}
void gradientEnds(const Node *gradient,float (&start)[4],float (&end)[4]) {
  if(!gradient)return;
  const auto colors=std::clamp<i64>(integer(*gradient,"m_NumColorKeys",2),1,8),alphas=std::clamp<i64>(integer(*gradient,"m_NumAlphaKeys",2),1,8);
  const auto color=[&](i64 index,float (&out)[4]) {
    if(const auto *key=gradient->get("key"+std::to_string(index)))
      for(u32 c=0;c<3;++c)out[c]=unityLinearFromSrgb(num(*key,std::string_view("rgb").substr(c,1),1));
  };
  const auto alpha=[&](i64 index)->float {const auto *key=gradient->get("key"+std::to_string(index));return key?std::clamp(num(*key,"a",1),0.f,1.f):1.f;};
  color(0,start);color(colors-1,end);start[3]=alpha(0);end[3]=alpha(alphas-1);
}
}

bool convertUnityPrefab(const UnityYamlDocument &document,UnityPrefabImport &out,std::string &diagnostic,const UnityPrefabContext *context) {
  UnityPrefabImport candidate;std::map<i64,usize> gameObjects;std::map<i64,i64> transforms;
  std::set<i64> ownedComponents,emptyRenderers;std::map<i64,std::set<i64>> declaredComponents;
  auto &coverage=candidate.coverage;
  const auto loss=[&](std::string text){note(coverage.losses,std::move(text));};
  const auto approximate=[&](std::string text){note(coverage.approximations,std::move(text));};
  for(const auto &object:document.objects) {
    if(object.stripped)return fail(diagnostic,"Prefab Unity contém objeto stripped/instância; patches precisam de conversão específica.");
    if(object.classId==1001) {
      // Unity até 2018.2 chama o pai de m_ParentPrefab; a 2018.2 de transição,
      // de m_SourcePrefab. Sem pai, o asset é o próprio prefab.
      const auto *mod=object.value.get("m_Modification");UnityObjectReference parent;
      if(object.type!="Prefab"||!(ref(object.value,"m_ParentPrefab",parent)||ref(object.value,"m_SourcePrefab",parent))||
         parent.fileId||parent.external()||!mod)
        return fail(diagnostic,"PrefabVariant/PrefabInstance Unity exige aplicação de patches por fileID; fonte preservada.");
      // Sem prefab pai, a lista de modificações é registro antigo do editor:
      // os objetos do arquivo já trazem os valores. Peças removidas, não.
      if(const auto *removed=mod->get("m_RemovedComponents");removed&&!removed->empty())
        return fail(diagnostic,"Prefab Unity contém componentes removidos não materializados.");
      if(const auto *modifications=mod->get("m_Modifications");modifications&&!modifications->empty())
        note(coverage.approximations,"Registro antigo de modificações do prefab ignorado (os objetos já trazem os valores).");
      UnityObjectReference root;if(!ref(object.value,"m_RootGameObject",root)||root.external()||!root.fileId||candidate.rootFileId)
        return fail(diagnostic,"Raiz de prefab Unity inválida ou duplicada.");
      candidate.rootFileId=root.fileId;continue;
    }
    // G6a: os componentes de efeito também têm consumidor; o resto continua
    // recusado com o classID, e scripts sem contexto do pacote também.
    static constexpr u32 known[]{1,4,23,33,198,199,108,82,54,50,135,136,65,58,70,61,120,114};
    if(std::find(std::begin(known),std::end(known),object.classId)==std::end(known)||(object.classId==114&&(!context||!context->scriptName)))
      return fail(diagnostic,"Componente Unity sem consumidor neste conversor de prefab: classID "+std::to_string(object.classId)+" · "+object.type+" · fileID "+std::to_string(object.fileId));
    if(object.classId!=1)continue;
    if(object.type!="GameObject")return fail(diagnostic,"ClassID Unity não corresponde ao tipo GameObject.");
    UnityPrefabObject value;value.fileId=object.fileId;
    const auto *name=object.value.get("m_Name"),*tag=object.value.get("m_TagString"),*layer=object.value.get("m_Layer"),*components=object.value.get("m_Component");i64 number=0;
    if(!name||name->kind!=Node::Kind::Scalar||name->scalar.empty()||name->scalar.size()>=64||
        !tag||tag->kind!=Node::Kind::Scalar||!layer||!layer->integer(number)||number<0||number>31||
        !components||components->kind!=Node::Kind::Sequence||!flag(object.value,"m_IsActive",value.active))
      return fail(diagnostic,"Nome/tag/layer/componentes de GameObject Unity inválidos ou fora do contrato nativo.");
    value.name=name->scalar;value.tag=tag->scalar;value.layer=static_cast<u32>(number);
    auto &declared=declaredComponents[value.fileId];
    for(const auto &component:components->elements) {
      UnityObjectReference reference;const auto *node=component.get("component");
      if(!node||!unityObjectReference(*node,reference)||reference.external()||!reference.fileId||!declared.insert(reference.fileId).second)
        return fail(diagnostic,"Referência de componente Unity inválida ou duplicada.");
    }
    gameObjects.emplace(value.fileId,candidate.objects.size());candidate.objects.push_back(std::move(value));
  }
  if(candidate.objects.empty())return fail(diagnostic,"Prefab Unity sem GameObjects completos.");
  // Dono de cada componente (sub-emissores e luz modelo apontam para o
  // componente, não para o GameObject).
  std::map<i64,i64> componentOwner;
  std::map<i64,const Node*> systems,renderers;
  std::map<i64,std::vector<const UnityYamlObject*>> behaviours;
  for(const auto &object:document.objects) {
    if(object.classId==1||object.classId==1001)continue;
    UnityObjectReference owner;
    if(!ref(object.value,"m_GameObject",owner)||owner.external()||!gameObjects.contains(owner.fileId)||
       !declaredComponents[owner.fileId].contains(object.fileId)||!ownedComponents.insert(object.fileId).second)
      return fail(diagnostic,"Componente Unity aponta para GameObject ausente ou não o declara.");
    componentOwner[object.fileId]=owner.fileId;
    auto &value=candidate.objects[gameObjects.at(owner.fileId)];
    const auto &node=object.value;
    switch(object.classId) {
    case 4: {
      UnityObjectReference parent;
      if(object.type!="Transform"||value.transformId||!ref(node,"m_Father",parent)||parent.external()||
          !vector(node,"m_LocalPosition",value.position)||!vector(node,"m_LocalRotation",value.rotation)||
          !vector(node,"m_LocalScale",value.scale)||!normalizeRotationQuaternion(value.rotation.data()))
        return fail(diagnostic,"Transform Unity inválido, duplicado ou externo.");
      for(float scale:value.scale)if(std::abs(scale)<1e-5f||std::abs(scale)>1e5f)return fail(diagnostic,"Escala de prefab Unity singular ou fora do orçamento.");
      value.position[2]*=-1;value.rotation[0]*=-1;value.rotation[1]*=-1;
      value.transformId=object.fileId;value.parentTransform=parent.fileId;transforms.emplace(object.fileId,owner.fileId);
      break;
    }
    case 33:
      if(object.type!="MeshFilter"||value.mesh.fileId||!ref(node,"m_Mesh",value.mesh)||!value.mesh.fileId||!value.mesh.external())
        return fail(diagnostic,"MeshFilter Unity exige referência externa GUID/fileID de malha completa.");
      break;
    case 23: {
      if(object.type!="MeshRenderer"||value.renderer||!flag(node,"m_Enabled",value.rendererEnabled)||!flag(node,"m_ReceiveShadows",value.receiveShadow))
        return fail(diagnostic,"MeshRenderer Unity inválido ou duplicado.");
      i64 shadows=0;const auto *cast=node.get("m_CastShadows");
      if(!cast||!cast->integer(shadows)||shadows<0||shadows>3)return fail(diagnostic,"ShadowCastingMode Unity fora de Off/On/TwoSided/ShadowsOnly.");
      value.castShadows=static_cast<u32>(shadows);value.renderer=true;
      const auto *materials=node.get("m_Materials");
      // Sem material a Unity não desenha nada com este renderer.
      if(materials&&materials->kind==Node::Kind::Sequence&&materials->elements.empty()){value.renderer=false;emptyRenderers.insert(value.fileId);break;}
      if(!materials||materials->kind!=Node::Kind::Sequence||materials->elements.empty()||materials->elements.size()>256)
        return fail(diagnostic,"Materiais de MeshRenderer Unity ausentes ou acima do limite.");
      for(const auto &material:materials->elements) {
        UnityObjectReference reference;
        if(!unityObjectReference(material,reference)||!reference.external()||!reference.fileId)
          return fail(diagnostic,"Material Unity ausente ou embutido sem conversão.");
        value.materials.push_back(reference);
      }
      // PK-6: LightProbeUsage Off/BlendProbes e ReflectionProbeUsage
      // Off/BlendProbes/BlendProbesAndSkybox/Simple chegam iguais. Proxy Volume
      // (2) e Custom Provided (4) não existem aqui: viram Misturar sondas, com aviso.
      {
        i64 mode=1;const auto *probe=node.get("m_LightProbeUsage");
        if(probe&&(!probe->integer(mode)||mode<0||mode>4))return fail(diagnostic,"LightProbeUsage Unity inválido.");
        if(mode==2||mode==4) candidate.transformations.push_back("Light Probe Proxy Volume/Custom não existem na Astra; o renderer usa Misturar sondas.");
        value.lightProbes=mode==0?0u:1u;
        mode=1;probe=node.get("m_ReflectionProbeUsage");
        if(probe&&(!probe->integer(mode)||mode<0||mode>3))return fail(diagnostic,"ReflectionProbeUsage Unity fora de Off/BlendProbes/BlendProbesAndSkybox/Simple.");
        value.reflectionProbes=static_cast<u32>(mode);
      }
      candidate.transformations.push_back("Lightmaps e sondas assadas da Unity não são transportados; asse as sondas de novo na Astra.");
      break;
    }
    case 198:
      if(object.type!="ParticleSystem"||systems.contains(owner.fileId))return fail(diagnostic,"ParticleSystem Unity inválido ou duplicado.");
      systems[owner.fileId]=&node;break;
    case 199:
      if(object.type!="ParticleSystemRenderer"||renderers.contains(owner.fileId))return fail(diagnostic,"ParticleSystemRenderer Unity inválido ou duplicado.");
      renderers[owner.fileId]=&node;break;
    case 108: {
      if(value.light)return fail(diagnostic,"Light Unity duplicada no mesmo GameObject.");
      UnityPrefabLight light;
      light.type=static_cast<u32>(std::clamp<i64>(integer(node,"m_Type",2),0,3));
      if(const auto *color=node.get("m_Color"))for(u32 c=0;c<3;++c)light.color[c]=std::clamp(num(*color,std::string_view("rgb").substr(c,1),1),0.f,1.f);
      light.intensity=std::max(0.f,num(node,"m_Intensity",1));light.range=std::max(.01f,num(node,"m_Range",10));
      light.spotAngle=std::clamp(num(node,"m_SpotAngle",30),1.f,179.f);
      if(const auto *shadows=node.get("m_Shadows"))light.shadows=static_cast<u32>(std::clamp<i64>(integer(*shadows,"m_Type",0),0,2));
      light.enabled=boolean(node,"m_Enabled",true);
      if(light.type==3){loss("Luz de Área sem consumidor em tempo real: vira pontual.");light.type=2;}
      if(light.type==0||light.type==1)approximate("Luz spot/direcional importada aponta para −Z do objeto (a frente da Unity refletida); confira a direção.");
      value.light=light;++coverage.lights;break;
    }
    case 82: {
      if(value.audio)return fail(diagnostic,"AudioSource Unity duplicada no mesmo GameObject.");
      UnityPrefabAudio audio;ref(node,"m_audioClip",audio.clip);
      audio.enabled=boolean(node,"m_Enabled",true);audio.playOnAwake=boolean(node,"m_PlayOnAwake",true);
      audio.loop=boolean(node,"Loop",false);audio.mute=boolean(node,"Mute",false);
      audio.volume=std::clamp(num(node,"m_Volume",1),0.f,1.f);audio.pitch=std::clamp(num(node,"m_Pitch",1),.01f,3.f);
      audio.minDistance=std::max(.01f,num(node,"MinDistance",1));audio.maxDistance=std::max(audio.minDistance+.01f,num(node,"MaxDistance",500));
      audio.priority=std::clamp(std::floor(num(node,"Priority",128)),0.f,256.f);
      audio.rolloff=static_cast<u32>(std::clamp<i64>(integer(node,"rolloffMode",0),0,2));
      if(const auto *pan=node.get("panLevelCustomCurve")) {float first=0,last=0;curveEnds(pan,1,first,last);audio.spatialBlend=std::clamp(first,0.f,1.f);}
      if(audio.rolloff==2)approximate("Queda de volume personalizada (curva) vira a logarítmica.");
      value.audio=audio;++coverage.audioSources;break;
    }
    case 54: case 50: {
      if(value.body)return fail(diagnostic,"Rigidbody Unity duplicado no mesmo GameObject.");
      UnityPrefabBody body;body.twoD=object.classId==50;
      body.mass=std::clamp(num(node,"m_Mass",1),.001f,1e6f);
      if(body.twoD) {
        body.kinematic=integer(node,"m_BodyType",boolean(node,"m_IsKinematic",false)?1:0)==1;
        body.drag=std::max(0.f,num(node,"m_LinearDrag",0));body.angularDrag=std::max(0.f,num(node,"m_AngularDrag",.05f));
        body.gravityScale=num(node,"m_GravityScale",1);
      } else {
        body.kinematic=boolean(node,"m_IsKinematic",false);body.gravity=boolean(node,"m_UseGravity",true);
        body.drag=std::max(0.f,num(node,"m_Drag",0));body.angularDrag=std::max(0.f,num(node,"m_AngularDrag",.05f));
      }
      value.body=body;++coverage.bodies;break;
    }
    case 135: case 136: case 65: case 58: case 70: case 61: {
      UnityPrefabCollider collider;collider.twoD=object.classId==58||object.classId==70||object.classId==61;
      collider.trigger=boolean(node,"m_IsTrigger",false);collider.enabled=boolean(node,"m_Enabled",true);
      if(collider.twoD) {
        std::array<float,2> offset{},size{1,1};vector(node,"m_Offset",offset);vector(node,"m_Size",size);
        collider.center[0]=offset[0];collider.center[1]=offset[1];collider.size[0]=size[0];collider.size[1]=size[1];
        collider.shape=object.classId==58?UnityPrefabCollider::Shape::Sphere:object.classId==70?UnityPrefabCollider::Shape::Capsule:UnityPrefabCollider::Shape::Box;
        collider.radius=std::max(0.f,num(node,"m_Radius",.5f));collider.direction=integer(node,"m_Direction",0)==1?0u:1u;
      } else {
        vec3(node,"m_Center",collider.center);collider.center[2]=-collider.center[2];
        collider.shape=object.classId==135?UnityPrefabCollider::Shape::Sphere:object.classId==136?UnityPrefabCollider::Shape::Capsule:UnityPrefabCollider::Shape::Box;
        collider.radius=std::max(0.f,num(node,"m_Radius",.5f));collider.height=std::max(0.f,num(node,"m_Height",2));
        collider.direction=static_cast<u32>(std::clamp<i64>(integer(node,"m_Direction",1),0,2));
        if(object.classId==65){std::array<float,3> size{1,1,1};vector(node,"m_Size",size);for(u32 c=0;c<3;++c)collider.size[c]=std::abs(size[c]);}
      }
      value.colliders.push_back(collider);++coverage.colliders;break;
    }
    case 120: {
      if(value.line)return fail(diagnostic,"LineRenderer Unity duplicado no mesmo GameObject.");
      UnityPrefabLine line;line.enabled=boolean(node,"m_Enabled",true);
      line.worldSpace=boolean(node,"m_UseWorldSpace",true);line.loop=boolean(node,"m_Loop",false);
      if(const auto *positions=node.get("m_Positions");positions&&positions->kind==Node::Kind::Sequence)
        for(const auto &point:positions->elements) {
          if(line.points.size()>=8){loss("Linha com mais de 8 pontos: os seguintes são ignorados.");break;}
          line.points.push_back({num(point,"x",0),num(point,"y",0),-num(point,"z",0)});
        }
      while(line.points.size()<2)line.points.push_back({0,0,line.points.empty()?0.f:-1.f});
      if(const auto *parameters=node.get("m_Parameters")) {
        curveEnds(parameters->get("widthCurve"),num(*parameters,"widthMultiplier",1),line.widthStart,line.widthEnd);
        gradientEnds(parameters->get("colorGradient"),line.colorStart,line.colorEnd);
        line.tile=integer(*parameters,"textureMode",0)!=0;
        if(integer(*parameters,"alignment",0)!=0)approximate("Linha alinhada ao Transform vira virada para a câmera.");
        if(integer(*parameters,"numCornerVertices",0)>0||integer(*parameters,"numCapVertices",0)>0)approximate("Cantos e pontas arredondados da Linha ficam retos.");
      }
      if(const auto *materials=node.get("m_Materials");materials&&materials->kind==Node::Kind::Sequence&&!materials->elements.empty())
        unityObjectReference(materials->elements.front(),line.material);
      value.line=line;++coverage.lines;break;
    }
    case 114: behaviours[owner.fileId].push_back(&object);break;
    default: break;
    }
  }
  // Sistemas de partículas: o sistema e o renderer do mesmo GameObject.
  for(const auto &[owner,node]:systems) {
    auto &value=candidate.objects[gameObjects.at(owner)];
    UnityParticleImport particle;
    const auto renderer=renderers.find(owner);
    if(!convertUnityParticleSystem(*node,renderer==renderers.end()?nullptr:renderer->second,particle,diagnostic))
      return fail(diagnostic,value.name+": "+diagnostic);
    for(const auto &text:particle.losses)loss(text);
    for(const auto &text:particle.approximations)approximate(text);
    value.particle=std::move(particle);++coverage.particleSystems;
  }
  for(const auto &[owner,node]:renderers)if(!systems.contains(owner))loss("Particle System Renderer sem Particle System no mesmo objeto.");
  // Referências locais do sistema traduzidas para GameObject.
  const auto ownerOf=[&](i64 component)->i64 {const auto found=componentOwner.find(component);return found==componentOwner.end()?0:found->second;};
  for(auto &value:candidate.objects) {
    if(!value.particle)continue;
    auto &p=*value.particle;
    for(auto &plane:p.collisionPlanes)if(plane){const auto found=transforms.find(plane);plane=found==transforms.end()?0:found->second;}
    for(i64 *sub:{&p.subEmitterBirth,&p.subEmitterCollision,&p.subEmitterDeath})*sub=*sub?ownerOf(*sub):0;
    for(auto &extra:p.extraSubEmitters)extra.first=extra.first?ownerOf(extra.first):0;
    p.lightTemplate=p.lightTemplate?ownerOf(p.lightTemplate):0;
    // Quantos o sub-sistema emite por acontecimento: as rajadas dele (a Unity
    // dispara a emissão do sub-sistema no evento), no mínimo uma partícula.
    const auto count=[&](i64 owner)->float {
      const auto found=gameObjects.find(owner);if(found==gameObjects.end())return 1;
      const auto &other=candidate.objects[found->second];if(!other.particle)return 1;
      float total=0;const auto &s=other.particle->settings;
      for(usize b=0;b<s.activeBursts();++b)total+=std::max(s.bursts[b].countMin,s.bursts[b].countMax);
      return std::clamp(std::floor(total),1.f,1000.f);
    };
    p.settings.subEmitterBirthCount=count(p.subEmitterBirth);
    p.settings.subEmitterCollisionCount=count(p.subEmitterCollision);
    p.settings.subEmitterDeathCount=count(p.subEmitterDeath);
  }
  // Na Unity o sub-emissor só emite pelos acontecimentos do pai, nunca sozinho.
  for(const auto &value:candidate.objects) {
    if(!value.particle||!value.particle->settings.subEmittersEnabled)continue;
    std::vector<i64> subs{value.particle->subEmitterBirth,value.particle->subEmitterCollision,value.particle->subEmitterDeath};
    for(const auto &extra:value.particle->extraSubEmitters)subs.push_back(extra.first);
    for(const i64 sub:subs) {
      const auto found=gameObjects.find(sub);
      if(sub&&found!=gameObjects.end()&&candidate.objects[found->second].particle)
        candidate.objects[found->second].particle->settings.playOnAwake=false;
    }
  }
  // Scripts reconhecidos pelo nome do .cs; os outros ficam de fora, listados.
  for(const auto &[owner,list]:behaviours) {
    auto &value=candidate.objects[gameObjects.at(owner)];
    for(const auto *object:list) {
      const auto &node=object->value;UnityObjectReference script;
      if(!ref(node,"m_Script",script)||!script.external())return fail(diagnostic,"MonoBehaviour Unity sem script de projeto.");
      const auto name=context->scriptName(script.guid);
      ++coverage.scripts;
      if(!boolean(node,"m_Enabled",true)){approximate("Script desligado no prefab não é convertido: "+(name.empty()?script.guid.text():name)+".");continue;}
      if(name=="AutoDestroyPS") {
        if(!value.particle){loss("AutoDestroyPS sem Particle System no objeto.");continue;}
        value.particle->settings.stopAction=scene::ParticleStopAction::Destroy;
        if(value.particle->settings.looping)approximate("AutoDestroyPS num sistema em laço: na Astra o objeto some quando o sistema para.");
      } else if(name=="DespawnObjectUniStorm") {
        // UniStorm: devolve o objeto ao pool (SetActive false) depois de
        // Seconds; na Astra, um Temporizador que desativa o próprio objeto.
        value.despawnSeconds=std::clamp(num(node,"Seconds",3),.05f,3600.0f);
      } else if(name=="ProjectileMover"||name=="ProjectileMover2D") {
        UnityPrefabProjectile projectile;projectile.twoD=name=="ProjectileMover2D";
        projectile.speed=std::max(0.f,num(node,"speed",15));projectile.hitOffset=num(node,"hitOffset",0);
        projectile.faceShooter=boolean(node,"UseFirePointRotation",false);
        float rotation[3]{};vec3(node,"rotationOffset",rotation);reflectedEuler(rotation,projectile.rotationOffset);
        ref(node,"hit",projectile.hit);ref(node,"flash",projectile.flash);
        if(const auto *detached=node.get("Detached");detached&&detached->kind==Node::Kind::Sequence)
          for(const auto &entry:detached->elements){UnityObjectReference reference;if(unityObjectReference(entry,reference)&&!reference.external()&&reference.fileId)projectile.detached.push_back(reference.fileId);}
        if(projectile.detached.size()>4)loss("Projétil com mais de 4 filhos a soltar: os seguintes somem com ele.");
        value.projectile=projectile;
        if(projectile.twoD)approximate("ProjectileMover2D vira o Projétil com consulta à física 3D.");
      } else if(name=="ParticleCollisionInstance") {
        UnityPrefabCollisionSpawn spawn;
        if(const auto *effects=node.get("EffectsOnCollision");effects&&effects->kind==Node::Kind::Sequence)
          for(const auto &entry:effects->elements){UnityObjectReference reference;if(unityObjectReference(entry,reference)&&reference.external())spawn.effects.push_back(reference);}
        if(spawn.effects.size()>4)loss("Instanciar ao colidir com mais de 4 efeitos: os seguintes são ignorados.");
        spawn.destroyDelay=std::max(.05f,num(node,"DestroyTimeDelay",5));spawn.offset=num(node,"Offset",0);
        float rotation[3]{};vec3(node,"rotationOffset",rotation);reflectedEuler(rotation,spawn.rotationOffset);
        spawn.worldSpace=boolean(node,"UseWorldSpacePosition",false);spawn.rotationOffsetOnly=boolean(node,"useOnlyRotationOffset",true);
        spawn.faceEmitter=boolean(node,"UseFirePointRotation",false);spawn.destroyEmitter=boolean(node,"DestoyMainEffect",false);
        value.collisionSpawn=spawn;
        if(!value.particle)loss("ParticleCollisionInstance sem Particle System no objeto.");
      } else if(name=="EGA_EffectSound") {
        UnityPrefabEffectSound sound;sound.repeating=boolean(node,"Repeating",true);sound.interval=std::max(.05f,num(node,"RepeatTime",2));
        sound.startDelay=std::max(0.f,num(node,"StartTime",0));sound.randomVolume=boolean(node,"RandomVolume",false);
        sound.volumeMin=std::clamp(num(node,"minVolume",.4f),0.f,1.f);sound.volumeMax=std::clamp(num(node,"maxVolume",1),0.f,1.f);
        value.effectSound=sound;
        if(!value.audio)loss("EGA_EffectSound sem Audio Source no objeto.");
      } else if(name=="Hovl_Laser") {
        UnityPrefabLaser laser;UnityObjectReference hit;
        if(ref(node,"HitEffect",hit)&&!hit.external())laser.hitEffect=hit.fileId;
        laser.hitOffset=num(node,"HitOffset",0);laser.alignHit=boolean(node,"useLaserRotation",false);
        laser.maxLength=std::max(.01f,num(node,"MaxLength",20));laser.textureLength=std::max(0.f,num(node,"MainTextureLength",1));
        value.laser=laser;
        if(!value.line)loss("Hovl_Laser sem Line Renderer no objeto.");
        approximate("Laser: a rolagem do ruído pelo comprimento (NoiseTextureLength) segue a da imagem principal.");
      } else if(name=="Rotator") {
        // x/y/z graus somados ao localEulerAngles a cada 0,0167 s (InvokeRepeating).
        // Velocidade angular é pseudovetor: na reflexão em Z, X e Y trocam de sinal.
        constexpr float kTicksPerSecond=1.0f/.0167f;
        value.rotator=std::array<float,3>{-num(node,"x",0)*kTicksPerSecond,-num(node,"y",0)*kTicksPerSecond,num(node,"z",0)*kTicksPerSecond};
        const int axes=(num(node,"x",0)!=0)+(num(node,"y",0)!=0)+(num(node,"z",0)!=0);
        if(axes>1)approximate("Girar em mais de um eixo soma os ângulos de Euler da Astra (XYZ), não os da Unity (ZXY).");
      } else if(name.find("Demo")!=std::string::npos) {
        // Script da cena de demonstração do pacote (teclas, troca de efeito):
        // não faz parte da aparência do efeito.
        approximate("Script de demonstração "+name+" não é convertido (controle da cena de exemplo do pacote).");
      } else {
        loss("Script "+(name.empty()?"de GUID "+script.guid.text():name)+" sem equivalente na Astra: o efeito fica sem o comportamento dele.");
      }
    }
  }
  usize roots=0;
  for(auto &value:candidate.objects) {
    if(emptyRenderers.contains(value.fileId))value.mesh={};
    if(!value.transformId||value.renderer!=bool(value.mesh.fileId))
      return fail(diagnostic,"Prefab Unity contém componente ausente ou MeshFilter/MeshRenderer incompleto.");
    // Todo componente declarado precisa existir no arquivo.
    usize found=0;for(const auto id:declaredComponents[value.fileId])found+=ownedComponents.contains(id);
    if(found!=declaredComponents[value.fileId].size())return fail(diagnostic,"Prefab Unity declara componente ausente do arquivo.");
    if(!value.parentTransform) {++roots;if(candidate.rootFileId&&candidate.rootFileId!=value.fileId)return fail(diagnostic,"Raiz Unity não coincide com a hierarquia.");candidate.rootFileId=value.fileId;}
    else if(!transforms.contains(value.parentTransform))return fail(diagnostic,"Pai Transform Unity ausente.");
    std::set<i64> path;const UnityPrefabObject *cursor=&value;
    while(cursor->parentTransform) {
      if(!path.insert(cursor->transformId).second)return fail(diagnostic,"Ciclo na hierarquia Unity.");
      cursor=&candidate.objects[gameObjects.at(transforms.at(cursor->parentTransform))];
    }
    const auto *transform=document.find(value.transformId);const auto *children=transform->value.get("m_Children");std::set<i64> declared;
    if(!children||children->kind!=Node::Kind::Sequence)return fail(diagnostic,"Lista de filhos Transform Unity inválida.");
    for(const auto &child:children->elements) {UnityObjectReference reference;
      if(!unityObjectReference(child,reference)||reference.external()||!transforms.contains(reference.fileId)||!declared.insert(reference.fileId).second||
          candidate.objects[gameObjects.at(transforms.at(reference.fileId))].parentTransform!=value.transformId)return fail(diagnostic,"Lista de filhos e m_Father Unity divergem.");}
    usize count=0;for(const auto &child:candidate.objects)count+=child.parentTransform==value.transformId;
    if(count!=declared.size())return fail(diagnostic,"Transform Unity omitiu um filho declarado por m_Father.");
  }
  if(roots!=1)return fail(diagnostic,"Prefab Unity precisa de exatamente uma raiz.");
  candidate.transformations.push_back("Hierarquia e referências locais por fileID exato; coordenadas LH para RH por reflexão Z; escala preservada.");
  candidate.transformations.push_back("Flags Static, ícones e metadados exclusivos do editor Unity não transportados; o grafo nativo conserva somente o contrato de cena executável declarado.");
  if(coverage.particleSystems)candidate.transformations.push_back("Partículas: "+std::to_string(coverage.particleSystems)+" sistema(s) convertidos módulo a módulo para o Sistema de partículas da Astra.");
  for(const auto &text:coverage.losses)candidate.transformations.push_back("Perda: "+text);
  for(const auto &text:coverage.approximations)candidate.transformations.push_back("Aproximação: "+text);
  std::sort(candidate.transformations.begin(),candidate.transformations.end());candidate.transformations.erase(std::unique(candidate.transformations.begin(),candidate.transformations.end()),candidate.transformations.end());
  out=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::resources
