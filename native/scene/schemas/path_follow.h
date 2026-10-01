#pragma once
#include "scene/path_follow.h"
namespace ae::scene {
inline constexpr std::array<ComponentSchema,1> pathFollowSchemas{{
 {.type=&PathFollow::descriptor,.name="Path Follow",.description="Percorre curva em distância mundial e orienta +Z",.family=ComponentFamily::Logic,.structuralInPlay=PlayMutability::SafePoint,.consumer="runtime/scene_paths.cpp",.invalidates=Invalidate::Transform,.subfamily="Caminhos",.icon="component/path-follow",.searchTerms="Path Follow Caminho Curva Percurso",.reference="https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html",.apiName="PathFollow"}
}};
}
