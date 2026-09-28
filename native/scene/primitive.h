#pragma once
#include "core/base.h"
#include <array>
#include <string_view>

namespace ae::scene {
enum class PrimitiveType : u32 { Cube, Sphere, Capsule, Cylinder, Plane, Quad, Count };
inline constexpr bool validPrimitive(PrimitiveType type) {return static_cast<u32>(type)<6;}
inline constexpr std::array<std::string_view,6> primitiveIds{"cube","sphere","capsule","cylinder","plane","quad"};
inline constexpr std::array<const char*,6> primitiveNames{"Cubo","Esfera","Cápsula","Cilindro","Plano","Quad"};
}
