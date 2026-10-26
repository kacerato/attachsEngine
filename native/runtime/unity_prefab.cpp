#include "runtime/unity_prefab.h"
#include "core/rotation_math.h"
#include "scene/audio.h"
#include "scene/collider.h"
#include "scene/light.h"
#include "scene/mesh_renderer.h"
#include "scene/particle_system.h"
#include "scene/physics2d_components.h"
#include "scene/physics_body.h"
#include "scene/vfx_behaviors.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ae::runtime {
namespace {
// G6a: os componentes de efeito de um objeto convertido, na ordem dos
// requisitos (Linha antes do Laser, Partículas antes do Instanciar ao colidir,
// Fonte antes do Som repetido). Dependência ausente vira nota, não recusa.
bool addEffects(const resources::UnityPrefabObject &object,const std::map<i64,ObjectId> &mapping,const UnityPrefabResolver &resolve,
    const UnityPrefabEffectServices *services,SceneObject &value,std::string &diagnostic) {
  const auto fail=[&](const std::string &message){diagnostic=message;return false;};
  const auto note=[&](const std::string &text) {
    if(services&&services->notes&&std::find(services->notes->begin(),services->notes->end(),text)==services->notes->end())services->notes->push_back(text);
  };
  const auto approximate=[&](const std::string &text) {
    auto *list=services?services->approximations:nullptr;
    if(list&&std::find(list->begin(),list->end(),text)==list->end())list->push_back(text);
  };
  const auto local=[&](i64 fileId)->u64 {const auto found=mapping.find(fileId);return fileId&&found!=mapping.end()?found->second:0;};
  // Uma identidade só (material, prefab, áudio); falha vira nota.
  const auto single=[&](const resources::UnityObjectReference &reference,resources::AssetType type,const char *what)->resources::AssetGuid {
    if(!reference.external()||!reference.fileId)return {};
    std::vector<resources::AssetGuid> ids;std::string error;
    if(!resolve(reference,type,ids,error)||ids.size()!=1||!ids.front().valid()) {note(std::string(what)+" sem conversão ("+object.name+"): "+error);return {};}
    return ids.front();
  };
  const auto add=[&](const scene::ComponentType &type)->scene::ComponentValue* {
    auto *component=value.components.add(type);
    if(!component)diagnostic="Componente "+std::string(type.id)+" recusado em "+object.name;
    return component;
  };
  if(object.particle) {
    const auto &source=*object.particle;
    auto *system=static_cast<scene::ParticleSystem*>(add(scene::ParticleSystem::descriptor));if(!system)return false;
    static_cast<scene::ParticleSystemSettings&>(*system)=source.settings;
    system->collisionPlane0=local(source.collisionPlanes[0]);system->collisionPlane1=local(source.collisionPlanes[1]);
    system->subEmitterBirth=local(source.subEmitterBirth);system->subEmitterCollision=local(source.subEmitterCollision);
    system->subEmitterDeath=local(source.subEmitterDeath);system->lightTemplate=local(source.lightTemplate);
    for(usize k=0;k<source.extraSubEmitters.size();++k) {
      system->subEmitterExtra[k]=local(source.extraSubEmitters[k].first);system->subEmitterExtraType[k]=source.extraSubEmitters[k].second;
    }
    // Material: o do projeto e o que o shader do pacote passa ao componente.
    if(source.material.external()&&source.material.fileId) {
      if(source.material.guid.text()=="0000000000000000f000000000000000") {
        approximate("Material embutido da Unity (Default-Particle) vira o ponto suave da Astra.");
      } else {
        system->materialAsset=single(source.material,resources::AssetType::Material,"Material de partícula");
        resources::UnityParticleLook look;
        if(services&&services->particleLook&&services->particleLook(source.material,look)) {
          system->materialBlend=look.family==resources::UnityParticleFamily::Additive?scene::ParticleBlend::Additive:
                                look.family==resources::UnityParticleFamily::Distortion?scene::ParticleBlend::Distortion:
                                scene::ParticleBlend::AlphaBlend;
          system->materialDistortion=look.distortion;
          if(look.family==resources::UnityParticleFamily::Distortion)
            approximate("Distorção copia a cena antes das partículas: as de trás não entram na imagem entortada.");
          system->materialEmission=std::clamp(look.emission*std::min(look.opacity,1.0f),0.0f,100.0f);
          std::copy(look.scroll,look.scroll+2,system->materialScroll);
          if(look.noise) {
            system->materialNoise=scene::ParticleNoise::Clouds;
            std::copy(look.noiseScroll,look.noiseScroll+2,system->materialNoiseScroll);
          }
          system->materialCenterGlow=look.centerGlow?1.0f:0.0f;
          system->materialSoftDistance=look.soft?.5f:0.0f;
        }
      }
    }
    if(source.mesh.external()&&source.mesh.fileId) {
      std::vector<resources::AssetGuid> meshes;std::string error;
      const auto &meshResolver=services&&services->particleMesh?services->particleMesh:resolve;
      if(meshResolver(source.mesh,resources::AssetType::Mesh,meshes,error)&&!meshes.empty())system->meshAsset=meshes.front();
      else note("Malha de partícula sem conversão ("+object.name+"): o sistema não desenha. "+error);
    }
    if(!system->valid())return fail("Sistema de partículas convertido inválido: "+object.name);
  }
  if(object.light) {
    const auto &source=*object.light;
    auto *light=static_cast<scene::Light*>(add(scene::Light::descriptor));if(!light)return false;
    light->kind=source.type==0?scene::LightKind::Spot:source.type==1?scene::LightKind::Directional:scene::LightKind::Point;
    // Intensidade da Unity (Built-in) é um multiplicador da cor: unidade Engine.
    light->unit=scene::LightUnit::Engine;light->intensity=source.intensity;light->range=source.range;
    std::copy(source.color,source.color+3,light->color);
    light->spotAngle=source.spotAngle;light->innerSpotAngle=std::min(light->innerSpotAngle,source.spotAngle);
    light->shadowMode=source.shadows;light->enabled=source.enabled;
    if(!light->valid())return fail("Luz convertida inválida: "+object.name);
  }
  if(object.audio) {
    const auto &source=*object.audio;
    auto *audio=static_cast<scene::AudioSource*>(add(scene::AudioSource::descriptor));if(!audio)return false;
    audio->clip=single(source.clip,resources::AssetType::AudioClip,"Clipe de áudio");
    audio->enabled=source.enabled;audio->loop=source.loop;audio->mute=source.mute;audio->volume=source.volume;audio->pitch=source.pitch;
    audio->minDistance=source.minDistance;audio->maxDistance=source.maxDistance;audio->spatialBlend=source.spatialBlend;audio->priority=source.priority;
    audio->playback=source.playOnAwake?scene::AudioPlayback::Playing:scene::AudioPlayback::Stopped;
    audio->dimension=source.spatialBlend>0?scene::AudioDimension::Spatial:scene::AudioDimension::Flat;
    audio->rolloff=source.rolloff==1?scene::AudioRolloff::Linear:scene::AudioRolloff::Inverse;
    if(!audio->valid())return fail("Fonte de áudio convertida inválida: "+object.name);
  }
  // O Projétil anda sozinho (consulta esférica): corpo e colisor da Unity ficam
  // de fora e o raio vem do colisor esférico.
  float projectileRadius=.1f;
  if(object.projectile) {
    for(const auto &collider:object.colliders)if(collider.shape==resources::UnityPrefabCollider::Shape::Sphere) {
      const float scale=std::max({std::abs(object.scale[0]),std::abs(object.scale[1]),std::abs(object.scale[2])});
      projectileRadius=collider.radius*scale;break;
    }
    if(object.body||!object.colliders.empty())approximate("Projétil: o Rigidbody e o colisor do pacote viram o raio da consulta do Projétil.");
  } else {
    if(object.body) {
      const auto &source=*object.body;
      if(source.twoD) {
        auto *body=static_cast<scene::Body2D*>(add(scene::Body2D::descriptor));if(!body)return false;
        body->motion=source.kinematic?scene::Body2DMotion::Kinematic:scene::Body2DMotion::Dynamic;
        body->mass=source.mass;body->gravityScale=source.gravityScale;body->linearDamping=source.drag;body->angularDamping=source.angularDrag;
      } else {
        auto *body=static_cast<scene::PhysicsBody*>(add(scene::PhysicsBody::descriptor));if(!body)return false;
        body->motion=source.kinematic?scene::BodyMotion::Kinematic:scene::BodyMotion::Dynamic;
        body->mass=source.mass;body->linearDamping=source.drag;body->angularDamping=source.angularDrag;body->gravityFactor=source.gravity?1.0f:0.0f;
        for(const auto &collider:object.colliders)if(collider.trigger&&!collider.twoD)body->sensor=true;
      }
    }
    for(const auto &source:object.colliders) {
      if(source.twoD) {
        auto *collider=static_cast<scene::Collider2D*>(add(scene::Collider2D::descriptor));if(!collider)return false;
        collider->shape=source.shape==resources::UnityPrefabCollider::Shape::Sphere?scene::Collider2DShape::Circle:
                        source.shape==resources::UnityPrefabCollider::Shape::Capsule?scene::Collider2DShape::Capsule:scene::Collider2DShape::Box;
        collider->halfX=source.size[0]*.5f;collider->halfY=source.size[1]*.5f;collider->radius=source.radius;
        collider->capsuleHalfLength=std::max(0.0f,std::max(source.size[0],source.size[1])*.5f-std::min(source.size[0],source.size[1])*.5f);
        if(source.shape==resources::UnityPrefabCollider::Shape::Capsule)collider->radius=std::min(source.size[0],source.size[1])*.5f;
        collider->offsetX=source.center[0];collider->offsetY=source.center[1];collider->sensor=source.trigger;
      } else {
        auto *collider=static_cast<scene::Collider*>(add(scene::Collider::descriptor));if(!collider)return false;
        collider->shape=source.shape==resources::UnityPrefabCollider::Shape::Sphere?scene::ColliderShape::Sphere:
                        source.shape==resources::UnityPrefabCollider::Shape::Capsule?scene::ColliderShape::Capsule:scene::ColliderShape::Box;
        collider->halfX=std::max(.001f,source.size[0]*.5f);collider->halfY=std::max(.001f,source.size[1]*.5f);collider->halfZ=std::max(.001f,source.size[2]*.5f);
        collider->radius=std::max(.001f,source.radius);collider->halfHeight=std::max(0.0f,source.height*.5f-source.radius);
        collider->centerX=source.center[0];collider->centerY=source.center[1];collider->centerZ=source.center[2];
        // Cápsula da Astra fica em Y; X e Z da Unity giram 90° (Z refletido).
        if(source.shape==resources::UnityPrefabCollider::Shape::Capsule&&source.direction==0)collider->rotationZ=90;
        if(source.shape==resources::UnityPrefabCollider::Shape::Capsule&&source.direction==2)collider->rotationX=90;
        collider->enabled=source.enabled;
        if(source.trigger&&!object.body)note("Colisor gatilho sem Rigidbody vira colisor sólido estático.");
      }
    }
  }
  if(object.line) {
    const auto &source=*object.line;
    auto *line=static_cast<scene::LineRenderer*>(add(scene::LineRenderer::descriptor));if(!line)return false;
    line->enabled=source.enabled;line->worldSpace=source.worldSpace;line->loop=source.loop;
    line->pointCount=static_cast<float>(std::clamp<usize>(source.points.size(),2,scene::LineRenderer::kMaximumPoints));
    for(usize i=0;i<source.points.size()&&i<scene::LineRenderer::kMaximumPoints;++i)
      for(u32 c=0;c<3;++c)line->points[i*3+c]=std::clamp(source.points[i][c],-100000.0f,100000.0f);
    line->widthStart=std::clamp(source.widthStart,0.0f,1000.0f);line->widthEnd=std::clamp(source.widthEnd,0.0f,1000.0f);
    std::copy(source.colorStart,source.colorStart+4,line->colorStart);std::copy(source.colorEnd,source.colorEnd+4,line->colorEnd);
    for(u32 c=0;c<3;++c){line->colorStart[c]=std::min(line->colorStart[c],65504.0f);line->colorEnd[c]=std::min(line->colorEnd[c],65504.0f);}
    line->tileLength=source.tile?1.0f:0.0f;
    line->material=single(source.material,resources::AssetType::Material,"Material da linha");
    resources::UnityParticleLook look;
    if(services&&services->particleLook&&source.material.external()&&services->particleLook(source.material,look)) {
      line->emission=std::clamp(look.emission*std::min(look.opacity,1.0f),0.0f,100.0f);
      line->scrollX=std::clamp(look.scroll[0],-100.0f,100.0f);line->scrollY=std::clamp(look.scroll[1],-100.0f,100.0f);
    }
    if(!line->valid())return fail("Linha convertida inválida: "+object.name);
  }
  if(object.laser) {
    const auto &source=*object.laser;
    if(!object.line) note("Laser sem Line Renderer no pacote: "+object.name+" fica sem o laser.");
    else {
      auto *laser=static_cast<scene::Laser*>(add(scene::Laser::descriptor));if(!laser)return false;
      laser->maxLength=std::clamp(source.maxLength,.01f,10000.0f);laser->hitOffset=std::clamp(source.hitOffset,-10.0f,10.0f);
      laser->alignHit=source.alignHit;laser->textureTiling=std::clamp(source.textureLength,0.0f,100.0f);
      laser->hitEffect=local(source.hitEffect);laser->forwardAxis=scene::VfxForwardAxis::NegativeZ;
    }
  }
  if(object.projectile) {
    const auto &source=*object.projectile;
    auto *projectile=static_cast<scene::Projectile*>(add(scene::Projectile::descriptor));if(!projectile)return false;
    projectile->speed=std::clamp(source.speed,0.0f,10000.0f);projectile->hitOffset=std::clamp(source.hitOffset,-10.0f,10.0f);
    projectile->radius=std::clamp(projectileRadius,0.0f,100.0f);projectile->faceShooter=source.faceShooter;
    std::copy(source.rotationOffset,source.rotationOffset+3,projectile->rotationOffset);
    projectile->forwardAxis=scene::VfxForwardAxis::NegativeZ;
    projectile->impact=single(source.hit,resources::AssetType::Prefab,"Prefab de impacto");
    projectile->muzzle=single(source.flash,resources::AssetType::Prefab,"Prefab de disparo");
    for(usize i=0;i<source.detached.size()&&i<scene::Projectile::kDetachSlots;++i)projectile->detach[i]=local(source.detached[i]);
  }
  if(object.collisionSpawn) {
    const auto &source=*object.collisionSpawn;
    if(!object.particle) note("Instanciar ao colidir sem Particle System: "+object.name+" fica sem ele.");
    else {
      auto *spawn=static_cast<scene::ParticleCollisionSpawn*>(add(scene::ParticleCollisionSpawn::descriptor));if(!spawn)return false;
      for(usize i=0;i<source.effects.size()&&i<scene::ParticleCollisionSpawn::kEffectSlots;++i)
        spawn->effects[i]=single(source.effects[i],resources::AssetType::Prefab,"Prefab de impacto");
      spawn->destroyDelay=std::clamp(source.destroyDelay,.05f,3600.0f);spawn->offset=std::clamp(source.offset,-10.0f,10.0f);
      std::copy(source.rotationOffset,source.rotationOffset+3,spawn->rotationOffset);
      spawn->worldSpace=source.worldSpace;spawn->rotationOffsetOnly=source.rotationOffsetOnly;spawn->faceEmitter=source.faceEmitter;
      spawn->destroyEmitter=source.destroyEmitter;spawn->forwardAxis=scene::VfxForwardAxis::NegativeZ;
    }
  }
  if(object.effectSound) {
    const auto &source=*object.effectSound;
    if(!object.audio) note("Som repetido sem Audio Source: "+object.name+" fica sem ele.");
    else {
      auto *sound=static_cast<scene::EffectSound*>(add(scene::EffectSound::descriptor));if(!sound)return false;
      sound->repeating=source.repeating;sound->randomVolume=source.randomVolume;sound->interval=std::clamp(source.interval,.05f,3600.0f);
      sound->startDelay=std::clamp(source.startDelay,0.0f,3600.0f);sound->volumeMin=source.volumeMin;sound->volumeMax=source.volumeMax;
    }
  }
  return true;
}
}

bool buildUnityPrefab(const resources::UnityPrefabImport &source,resources::AssetGuid guid,const UnityPrefabResolver &resolve,
    const UnityPrefabIdentityMap *previous,Prefab &out,UnityPrefabIdentityMap &identities,std::string &diagnostic,
    const UnityPrefabEffectServices *effects) {
  const auto fail=[&](const std::string &message){diagnostic=message;return false;};
  if(!guid.valid()||source.objects.empty()||source.objects.size()>SceneGraph::kMaximumObjects-1||!resolve)return fail("Prefab convertido ou resolvedor inválido.");
  UnityPrefabIdentityMap candidate=previous?*previous:UnityPrefabIdentityMap{};
  std::map<i64,ObjectId> mapping;std::set<ObjectId> used;
  for(const auto &entry:candidate.entries)if(!entry.fileId||entry.object<2||entry.object>=candidate.nextObject||
      !mapping.emplace(entry.fileId,entry.object).second||!used.insert(entry.object).second)return fail("Mapa persistente de fileIDs Unity inválido.");
  for(const auto &object:source.objects)if(!mapping.contains(object.fileId)) {
    if(candidate.nextObject>SceneGraph::kMaximumObjects)return fail("Identidades de prefab esgotadas; IDs removidos não são reciclados.");
    const auto id=candidate.nextObject++;candidate.entries.push_back({object.fileId,id});mapping.emplace(object.fileId,id);
  }
  SceneGraph graph;std::map<i64,const resources::UnityPrefabObject*> transforms;std::set<i64> pending;
  for(const auto &object:source.objects){transforms.emplace(object.transformId,&object);pending.insert(object.fileId);}
  while(!pending.empty()) {
    bool progressed=false;
    for(const auto &object:source.objects) {
      if(!pending.contains(object.fileId))continue;
      ObjectId parent=graph.root();
      if(object.parentTransform) {
        const auto found=transforms.find(object.parentTransform);if(found==transforms.end())return fail("Pai Unity ausente.");
        parent=mapping.at(found->second->fileId);if(!graph.exists(parent))continue;
      }
      if(object.tag!="Untagged"||object.layer)return fail("Tags/camadas Unity exigem mapa explícito do projeto: "+object.name);
      SceneObject value;value.id=mapping.at(object.fileId);value.parent=parent;
      value.kind=object.renderer?ObjectKind::Mesh:object.particle||object.line?ObjectKind::Effect:object.light?ObjectKind::Light:ObjectKind::Folder;
      assignObjectName(value,object.name);value.active=object.active;value.castShadow=object.castShadows!=0;value.receiveShadow=object.receiveShadow;
      std::copy(object.position.begin(),object.position.end(),value.transform.position);std::copy(object.scale.begin(),object.scale.end(),value.transform.scale);
      if(!rotationEulerXYZ(object.rotation.data(),value.transform.rotationDegrees))return fail("Rotação do prefab não representável.");
      if(object.renderer) {
        std::vector<resources::AssetGuid> meshes;
        if(!resolve(object.mesh,resources::AssetType::Mesh,meshes,diagnostic))return false;
        if(meshes.empty()||meshes.size()!=object.materials.size())return fail("Quantidade de submeshes/materiais Unity difere da malha convertida: "+object.name);
        auto *renderer=static_cast<scene::MeshRenderer*>(value.components.add(scene::MeshRenderer::descriptor));
        if(!renderer)return fail("Componente MeshRenderer nativo indisponível.");
        renderer->enabled=object.rendererEnabled;renderer->asset=meshes.front();
        renderer->castShadows=object.castShadows;renderer->receiveShadows=object.receiveShadow;
        renderer->lightProbes=object.lightProbes;renderer->reflectionProbes=object.reflectionProbes;
        for(usize slot=1;slot<meshes.size();++slot){scene::MeshSubmesh submesh;submesh.asset=meshes[slot];renderer->submeshes.push_back(submesh);}
        for(usize slot=0;slot<object.materials.size();++slot) {
          std::vector<resources::AssetGuid> materials;
          if(!resolve(object.materials[slot],resources::AssetType::Material,materials,diagnostic))return false;
          if(materials.size()!=1||!materials.front().valid())return fail("Material convertido ausente ou ambíguo.");
          *renderer->editSlotMaterialAsset(static_cast<u32>(slot))=materials.front();
        }
      }
      if(!addEffects(object,mapping,resolve,effects,value,diagnostic))return false;
      if(!graph.restoreEntity(value,std::numeric_limits<u32>::max()))return fail("Prefab recusado pelo grafo nativo: "+object.name);
      pending.erase(object.fileId);progressed=true;
    }
    if(!progressed)return fail("Ciclo ou pai ausente no prefab convertido.");
  }
  if(!mapping.contains(source.rootFileId)||!graph.reserveObjectIdsUntil(candidate.nextObject))return fail("Raiz ou fronteira de IDs inválida.");
  Prefab prefab;if(!prefab.capture(graph,mapping.at(source.rootFileId),guid,diagnostic))return false;
  out=std::move(prefab);identities=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::runtime
