#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

// Coverage geometry has already written its surviving alpha-tested samples to
// depth. This shading family uses depth EQUAL and never writes depth, so running
// the test before the fragment shader is both exact and essential on dense
// foliage: hidden cards must not execute normal/PBR work merely because the
// shared material shader contains discard.
layout(early_fragment_tests) in;
layout(set=0,binding=0) uniform sampler2D textures[];
#define BASE_MAP textures[frame.textureIndices.x]
#define NORMAL_MAP textures[frame.textureIndices.y]
#define MR_MAP textures[frame.textureIndices.z]
#define EMISSIVE_MAP textures[frame.textureIndices.w]
// R4: textura de oclusão própria, índice da extensão de material do desenho.
#define OCCLUSION_MAP(index) textures[index]
#include "dirt_road_shading.glsl"
