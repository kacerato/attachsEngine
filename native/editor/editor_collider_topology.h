#pragma once
#include "editor/editor_component_visuals.h"
#include "physics/collision_cooking.h"
#include <chrono>
#include <map>
#include <numeric>

namespace ae::editor {
// A private collision resource draft, never the renderer's mutable geometry.
// Indices are stable for this draft only; publication keeps the component UID.
struct ColliderTopology {
  static constexpr u32 MaximumTriangles = 100000, MaximumDrawTriangles = 800,
                       MaximumDrawVertices = 512;
  using Point = std::array<float, 3>;
  EditorEntityId object = 0;
  u64 instance = 0;
  std::vector<Point> vertices;
  std::vector<Point> sourceVertices;
  std::vector<u32> indices, faceOfTriangle;
  std::vector<std::vector<u32>> faces;
  std::vector<EditorPickMesh::Triangle> cooked;
  EditorPickMesh surface;
  std::vector<std::vector<Point>> undo, redo;
  bool vertexMode = false, hidden = false, additive = false, converted = false,
       convex = false, valid = false, dirty = false;
  std::vector<u32> selected;
  std::string error, sourceHash;
  double buildMs = 0, validateMs = 0, pickMs = 0;
  u64 cooks = 0, picks = 0;
  void clear() { *this = {}; }
  bool active() const { return object != 0; }
  static double ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
  }
  std::vector<u32> selectedVertices() const {
    std::vector<u32> result;
    for (auto id : selected)
      if (vertexMode) {
        if (id < vertices.size())
          result.push_back(id);
      } else if (id < faces.size())
        result.insert(result.end(), faces[id].begin(), faces[id].end());
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
  bool centroid(Point &out) const {
    const auto ids = selectedVertices();
    out = {};
    if (ids.empty())
      return false;
    for (auto id : ids)
      for (u32 k = 0; k < 3; ++k)
        out[k] += vertices[id][k] / ids.size();
    return true;
  }
  void checkpoint() {
    undo.push_back(vertices);
    if (undo.size() > 32)
      undo.erase(undo.begin());
    redo.clear();
  }
  bool travel(bool forward, float tolerance) {
    auto &from = forward ? redo : undo, &to = forward ? undo : redo;
    if (from.empty())
      return false;
    to.push_back(vertices);
    vertices = std::move(from.back());
    from.pop_back();
    dirty = true;
    validate(tolerance);
    return true;
  }
  bool translate(const std::vector<Point> &base, u32 axis, float delta) {
    if (axis > 2 || base.size() != vertices.size() || !std::isfinite(delta))
      return false;
    const auto ids = selectedVertices();
    if (ids.empty())
      return false;
    vertices = base;
    for (auto id : ids)
      vertices[id][axis] += delta;
    dirty = true;
    valid = false;
    error = "Prévia alterada; validar antes de aplicar";
    return true;
  }
  bool validate(float tolerance) {
    const auto start = std::chrono::steady_clock::now();
    valid = false;
    cooked.clear();
    error.clear();
    dirty = vertices != sourceVertices;
    for (const auto &p : vertices)
      for (auto n : p)
        if (!std::isfinite(n) || std::abs(n) > 1000000) {
          error =
              "Coordenada inválida ou fora do limite de um milhão de unidades";
          validateMs = ms(start);
          return false;
        }
    for (usize i = 0; i < indices.size(); i += 3) {
      const auto &a = vertices[indices[i]], &b = vertices[indices[i + 1]],
                 &c = vertices[indices[i + 2]];
      float u[3], v[3], n[3];
      for (u32 k = 0; k < 3; ++k) {
        u[k] = b[k] - a[k];
        v[k] = c[k] - a[k];
      }
      n[0] = u[1] * v[2] - u[2] * v[1];
      n[1] = u[2] * v[0] - u[0] * v[2];
      n[2] = u[0] * v[1] - u[1] * v[0];
      if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] < 1e-14f) {
        error = "A edição colapsou uma face; ajuste ou desfaça";
        validateMs = ms(start);
        return false;
      }
    }
    std::vector<EditorPickMesh::Triangle> raw;
    raw.reserve(indices.size() / 3);
    for (usize i = 0; i < indices.size(); i += 3) {
      EditorPickMesh::Triangle t;
      for (u32 v = 0; v < 3; ++v)
        std::copy_n(vertices[indices[i + v]].data(), 3, t.data() + v * 3);
      raw.push_back(t);
    }
    if (!surface.build(raw)) {
      error = "Não foi possível construir a seleção geométrica";
      validateMs = ms(start);
      return false;
    }
    if (convex) {
      std::vector<AetherVec3> points;
      points.reserve(vertices.size());
      for (const auto &p : vertices)
        points.push_back({p[0], p[1], p[2]});
      physics::CookedConvexHull hull;
      auto settings = AetherMeshCookingDefaultsV1;
      settings.hullTolerance = tolerance;
      ++cooks;
      if (!physics::cookConvexHull(points, settings, hull, error)) {
        validateMs = ms(start);
        return false;
      }
      for (usize i = 0; i < hull.indices.size(); i += 3) {
        EditorPickMesh::Triangle t;
        for (u32 v = 0; v < 3; ++v) {
          const auto &p = hull.vertices[hull.indices[i + v]];
          t[v * 3] = p.x;
          t[v * 3 + 1] = p.y;
          t[v * 3 + 2] = p.z;
        }
        cooked.push_back(t);
      }
    } else
      cooked = std::move(raw);
    valid = true;
    validateMs = ms(start);
    return true;
  }
  bool build(std::span<const EditorPickMesh::Triangle> soup,
             std::string &reason) {
    const auto start = std::chrono::steady_clock::now();
    if (soup.empty() || soup.size() > MaximumTriangles) {
      reason = "A edição aceita de 1 a 100.000 triângulos; divida o recurso de "
               "colisão";
      return false;
    }
    std::map<Point, u32> ids;
    for (const auto &t : soup)
      for (u32 v = 0; v < 3; ++v) {
        Point p;
        std::copy_n(t.data() + v * 3, 3, p.data());
        for (auto &n : p) {
          if (!std::isfinite(n)) {
            reason = "Vértice não finito";
            return false;
          }
          if (n == 0)
            n = 0;
        }
        const auto [it, inserted] =
            ids.emplace(p, static_cast<u32>(ids.size()));
        if (inserted)
          vertices.push_back(p);
        indices.push_back(it->second);
      }
    // Join adjacent coplanar triangles into polygon faces. Disconnected
    // surfaces with the same plane remain distinct; no all-pairs comparison.
    std::vector<u32> parent(soup.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](u32 i) {
      while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
      }
      return i;
    };
    std::vector<Point> normals(soup.size());
    std::vector<float> plane(soup.size());
    std::map<std::pair<u32, u32>, u32> edges;
    for (u32 i = 0; i < soup.size(); ++i) {
      const auto &a = vertices[indices[i * 3]],
                 &b = vertices[indices[i * 3 + 1]],
                 &c = vertices[indices[i * 3 + 2]];
      Point u, v, n;
      for (u32 k = 0; k < 3; ++k) {
        u[k] = b[k] - a[k];
        v[k] = c[k] - a[k];
      }
      n = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
           u[0] * v[1] - u[1] * v[0]};
      const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      if (length < 1e-7f) {
        reason = "Triângulo degenerado na origem";
        return false;
      }
      for (u32 k = 0; k < 3; ++k) {
        normals[i][k] = n[k] / length;
        plane[i] += normals[i][k] * a[k];
      }
      for (u32 e = 0; e < 3; ++e) {
        auto x = indices[i * 3 + e], y = indices[i * 3 + (e + 1) % 3];
        if (x > y)
          std::swap(x, y);
        const auto [it, added] = edges.emplace(std::pair{x, y}, i);
        if (!added) {
          const auto j = it->second;
          float dot = 0;
          for (u32 k = 0; k < 3; ++k)
            dot += normals[i][k] * normals[j][k];
          if (dot > .99999f && std::abs(plane[i] - plane[j]) <
                                   1e-5f * std::max(1.f, std::abs(plane[i])))
            parent[root(i)] = root(j);
        }
      }
    }
    std::map<u32, u32> faceIds;
    faceOfTriangle.resize(soup.size());
    for (u32 i = 0; i < soup.size(); ++i) {
      const auto [it, added] =
          faceIds.emplace(root(i), static_cast<u32>(faceIds.size()));
      if (added)
        faces.emplace_back();
      faceOfTriangle[i] = it->second;
      for (u32 v = 0; v < 3; ++v)
        faces[it->second].push_back(indices[i * 3 + v]);
    }
    for (auto &f : faces) {
      std::sort(f.begin(), f.end());
      f.erase(std::unique(f.begin(), f.end()), f.end());
    }
    sourceVertices = vertices;
    buildMs = ms(start);
    reason.clear();
    return true;
  }
};
inline bool colliderTopologyPose(const runtime::SceneGraph &graph,
                                 EditorEntityId object, u64 instance,
                                 float *pose) {
  const auto *e = graph.find(object);
  const auto *v = e ? e->components.findInstance(instance) : nullptr;
  if (!v || &v->type() != &scene::Collider::descriptor)
    return false;
  const auto &c = static_cast<const scene::Collider &>(*v);
  if (!editorWorldMatrix(graph, object, pose))
    return false;
  if (scene::colliderHasLocalPose(c)) {
    EditorTransform t;
    t.position[0] = c.centerX;
    t.position[1] = c.centerY;
    t.position[2] = c.centerZ;
    t.rotationDegrees[0] = c.rotationX;
    t.rotationDegrees[1] = c.rotationY;
    t.rotationDegrees[2] = c.rotationZ;
    float local[16];
    editorTransformMatrix(t, local);
    visual_detail::multiply(pose, local, pose);
  }
  return true;
}
inline ColliderTopology::Point
topologyWorldPoint(const float *pose, const ColliderTopology::Point &p) {
  ColliderTopology::Point out;
  for (u32 k = 0; k < 3; ++k)
    out[k] =
        pose[12 + k] + pose[k] * p[0] + pose[k + 4] * p[1] + pose[k + 8] * p[2];
  return out;
}
inline bool topologyRayTriangle(const EditorRay &ray,
                                const ColliderTopology::Point &a,
                                const ColliderTopology::Point &b,
                                const ColliderTopology::Point &c,
                                float &distance) {
  float e[3], f[3], p[3], t[3], q[3];
  for (u32 k = 0; k < 3; ++k) {
    e[k] = b[k] - a[k];
    f[k] = c[k] - a[k];
    t[k] = ray.origin[k] - a[k];
  }
  p[0] = ray.direction[1] * f[2] - ray.direction[2] * f[1];
  p[1] = ray.direction[2] * f[0] - ray.direction[0] * f[2];
  p[2] = ray.direction[0] * f[1] - ray.direction[1] * f[0];
  const float det = e[0] * p[0] + e[1] * p[1] + e[2] * p[2];
  if (std::abs(det) < 1e-9f)
    return false;
  const float u = (t[0] * p[0] + t[1] * p[1] + t[2] * p[2]) / det;
  if (u < -.00001f || u > 1.00001f)
    return false;
  q[0] = t[1] * e[2] - t[2] * e[1];
  q[1] = t[2] * e[0] - t[0] * e[2];
  q[2] = t[0] * e[1] - t[1] * e[0];
  const float v = (ray.direction[0] * q[0] + ray.direction[1] * q[1] +
                   ray.direction[2] * q[2]) /
                  det;
  if (v < -.00001f || u + v > 1.00001f)
    return false;
  distance = (f[0] * q[0] + f[1] * q[1] + f[2] * q[2]) / det;
  return distance >= ray.minimumDistance && distance <= ray.maximumDistance;
}
} // namespace ae::editor
