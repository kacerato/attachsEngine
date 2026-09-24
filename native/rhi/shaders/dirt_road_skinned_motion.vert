#version 450
#extension GL_GOOGLE_include_directive : require
// Passe de movimento de desenho com skin (G6-B): o atributo 2 (tangente, que o
// vetor de movimento nao usa) chega do binding 2 com a posicao deformada do
// quadro anterior, escrita pelo compute de skinning logo depois da pose atual.
#define AETHER_SKINNED_MOTION
#include "dirt_road_vertex.glsl"
