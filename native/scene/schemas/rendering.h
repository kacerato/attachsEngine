// Família Renderização: geometria, deformação, nível de detalhe e ambiente.
// Incluído somente por scene/component_schema.h.
#pragma once
#include "scene/environment.h"
#include "scene/lod_group.h"
#include "scene/mesh_renderer.h"
#include "scene/skinned_mesh.h"

namespace ae::scene {
inline constexpr std::array<ComponentRule, 1> skinnedMeshRequirements{{
  {"astra.render.mesh", "Adicione Malha a este objeto"}
}};
inline constexpr std::array<ComponentSchema, 4> renderingSchemas{{
  {.type=&MeshRenderer::descriptor, .name="Malha", .description="Geometria e material",
   .family=ComponentFamily::Rendering, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="renderer/map_draw_update.h → instância e material efetivo", .capability="render.material.pbr",
   .invalidates=Invalidate::Draw|Invalidate::MaterialDescriptor,
   .subfamily="Geometria", .icon="assets/static-mesh", .searchTerms="MeshFilter MeshRenderer MeshInstance3D",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html",
   .apiName="MeshRenderer"},
  {.type=&SkinnedMesh::descriptor, .name="Malha deformável", .description="Esqueleto e blend shapes da Malha",
   .family=ComponentFamily::Rendering, .requirements=skinnedMeshRequirements,
   .structuralInPlay=PlayMutability::SafePoint,
   .consumer="editor/editor_map_scene.cpp → paleta; platform/android/instanced_skinning.inl → compute",
   .capability="render.skinning", .invalidates=Invalidate::Draw|Invalidate::ShadowMap,
   .subfamily="Geometria", .icon="component/skinned-mesh", .searchTerms="SkinnedMeshRenderer Skeleton3D BlendShape Morph",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-SkinnedMeshRenderer.html",
   .apiName="SkinnedMesh"},
  {.type=&LodGroup::descriptor, .name="LOD Group", .description="Nível de detalhe pela altura na tela",
   .family=ComponentFamily::Rendering, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/lod_groups.h → visibilidade do desenho por vista", .capability="render.lod.group",
   .invalidates=Invalidate::Draw,
   .subfamily="Desempenho", .icon="component/lod-group", .searchTerms="LODGroup VisibilityRange Detalhe",
   .reference="https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html",
   .apiName="LodGroup"},
  {.type=&Environment::descriptor, .name="Ambiente", .description="Céu, atmosfera, neblina e pós globais ou por volume",
   .family=ComponentFamily::Rendering, .structuralInPlay=PlayMutability::SafePoint,
   .consumer="runtime/scene_environment.cpp → renderer e pós", .invalidates=Invalidate::Draw|Invalidate::Policy,
   .subfamily="Ambiente", .icon="lighting/sky", .searchTerms="Volume WorldEnvironment Skybox Fog PostProcess",
   .reference="https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/Volumes.html",
   .apiName="EnvironmentVolume"}
}};
} // namespace ae::scene
