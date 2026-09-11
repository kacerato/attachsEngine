// A convenção de transformação da cena, em um lugar só.
//
// Matrizes coluna-maior, Euler Rz * Ry * Rx em graus, escala aplicada por
// coluna. O editor, a física e os scripts precisam concordar bit a bit: uma
// segunda implementação da mesma conversão é como o gizmo e o corpo rígido
// passam a discordar de onde o objeto está.
#pragma once
#include "runtime/scene_graph.h"

namespace ae::runtime {

void multiplyMatrix(const float a[16], const float b[16], float out[16]);
void transformMatrix(const Transform &transform, float out[16]);

// Matriz de mundo acumulando todos os ancestrais. Falso para id inexistente ou
// resultado não finito — nunca devolve uma matriz parcialmente escrita válida.
bool worldMatrix(const SceneGraph &graph, ObjectId id, float out[16]);
// Matriz de mundo do PAI; identidade quando o objeto está na raiz da hierarquia.
bool parentWorldMatrix(const SceneGraph &graph, ObjectId id, float out[16]);

// Decompõe `world` no referencial de `parent`. Recusa shear e reflexão em vez de
// perder silenciosamente a transformação de mundo anterior.
bool localTransformForWorld(const float world[16], const float parent[16], Transform &out);

// Rotação (x,y,z,w) equivalente aos ângulos de Euler do transform.
void transformRotationQuaternion(const Transform &transform, float out[4]);

} // namespace ae::runtime
