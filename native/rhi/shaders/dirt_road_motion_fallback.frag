#version 450
#extension GL_GOOGLE_include_directive : require
layout(set=0,binding=0) uniform sampler2D baseMap;
#define BASE_MAP baseMap
#include "dirt_road_motion_common.glsl"
