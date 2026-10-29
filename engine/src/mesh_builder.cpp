#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <mesh_builder.hpp>
#include <random.hpp>
#include <unordered_map>
#include <vector>
namespace kuki {
namespace {
  /// @brief Any unit vector at right angles to a normal, for a tangent nothing will bend by: the
  /// shader normalises the tangent whether or not a normal map reads it, and a zero one is a NaN.
  auto Perpendicular(const glm::vec3 &n) -> glm::vec3 {
    const auto axis = std::abs(n.y) < .9f ? glm::vec3(.0f, 1.f, .0f) : glm::vec3(1.f, .0f, .0f);
    return glm::normalize(glm::cross(axis, n));
  }
} // namespace
auto MeshBuilder::Emit(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) -> void {
  auto pa = a;
  auto pb = b;
  auto pc = c;
  if (transformed) {
    pa = glm::vec3(transform * glm::vec4(a, 1.f));
    pb = glm::vec3(transform * glm::vec4(b, 1.f));
    pc = glm::vec3(transform * glm::vec4(c, 1.f));
  }
  const auto cross = glm::cross(pb - pa, pc - pa);
  const auto length = glm::length(cross);
  // A triangle with no area has no front and draws nothing; leaving it out keeps a NaN normal out
  // of the mesh, which the cel bands would show as a black speck.
  if (length < 1e-9f)
    return;
  const auto normal = cross / length;
  const auto tangent = Perpendicular(normal);
  for (const auto &p : {pa, pb, pc}) {
    Vertex vertex{};
    vertex.position = p;
    vertex.normal = normal;
    vertex.tangent = tangent;
    // Laid flat on the ground plane: what a texture would want if a material ever gave one.
    vertex.texture = {p.x * .25f, p.z * .25f};
    mesh.vertices.push_back(vertex);
  }
}
auto MeshBuilder::Triangle(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) -> MeshBuilder & {
  Emit(a, b, c);
  return *this;
}
auto MeshBuilder::Oriented(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &towards) -> MeshBuilder & {
  if (glm::dot(glm::cross(b - a, c - a), towards) < .0f)
    Emit(a, c, b);
  else
    Emit(a, b, c);
  return *this;
}
auto MeshBuilder::Quad(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d) -> MeshBuilder & {
  Emit(a, b, c);
  Emit(a, c, d);
  return *this;
}
auto MeshBuilder::OrientedQuad(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d, const glm::vec3 &towards) -> MeshBuilder & {
  Oriented(a, b, c, towards);
  Oriented(a, c, d, towards);
  return *this;
}
auto MeshBuilder::Fan(const glm::vec3 &middle, const std::span<const glm::vec3> ring, const glm::vec3 &towards) -> MeshBuilder & {
  for (size_t i = 0; i < ring.size(); ++i)
    Oriented(middle, ring[i], ring[(i + 1) % ring.size()], towards);
  return *this;
}
auto MeshBuilder::Frustum(const glm::vec3 &base, const float lower, const float upper, const float height, const int sides, const float turn) -> MeshBuilder & {
  const auto n = std::max(sides, 3);
  std::vector<glm::vec3> bottom;
  std::vector<glm::vec3> top;
  for (auto i = 0; i < n; ++i) {
    const auto angle = turn + static_cast<float>(i) / static_cast<float>(n) * 6.2831853f;
    const glm::vec3 direction{std::cos(angle), .0f, std::sin(angle)};
    bottom.push_back(base + direction * lower);
    top.push_back(base + direction * upper + glm::vec3(.0f, height, .0f));
  }
  const auto apex = base + glm::vec3(.0f, height, .0f);
  for (auto i = 0; i < n; ++i) {
    const auto j = (i + 1) % n;
    const auto outward = (bottom[i] + bottom[j]) * .5f - base;
    if (upper > .0f)
      OrientedQuad(bottom[i], bottom[j], top[j], top[i], outward);
    else
      Oriented(bottom[i], bottom[j], apex, outward + glm::vec3(.0f, lower * .01f, .0f));
  }
  Fan(base, bottom, {.0f, -1.f, .0f});
  if (upper > .0f)
    Fan(apex, top, {.0f, 1.f, .0f});
  return *this;
}
auto MeshBuilder::Box(const glm::vec3 &low, const glm::vec3 &high) -> MeshBuilder & {
  return Block((low + high) * .5f, (high - low) * .5f);
}
auto MeshBuilder::Block(const glm::vec3 &centre, const glm::vec3 &half, const float turn) -> MeshBuilder & {
  const auto c = std::cos(turn);
  const auto s = std::sin(turn);
  const auto Corner = [&](const float x, const float y, const float z) {
    const auto lx = x * half.x;
    const auto lz = z * half.z;
    return centre + glm::vec3(lx * c - lz * s, y * half.y, lx * s + lz * c);
  };
  const std::array<glm::vec3, 8> p{Corner(-1, -1, -1), Corner(1, -1, -1), Corner(1, -1, 1), Corner(-1, -1, 1), Corner(-1, 1, -1), Corner(1, 1, -1), Corner(1, 1, 1), Corner(-1, 1, 1)};
  constexpr std::array<std::array<int, 4>, 6> faces{{{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}};
  for (const auto &face : faces) {
    const auto middle = (p[face[0]] + p[face[1]] + p[face[2]] + p[face[3]]) * .25f;
    OrientedQuad(p[face[0]], p[face[1]], p[face[2]], p[face[3]], middle - centre);
  }
  return *this;
}
auto MeshBuilder::Gable(const glm::vec3 &centre, const glm::vec2 &half, const float height, const float turn) -> MeshBuilder & {
  const auto c = std::cos(turn);
  const auto s = std::sin(turn);
  const auto At = [&](const float x, const float y, const float z) {
    return centre + glm::vec3(x * c - z * s, y, x * s + z * c);
  };
  const auto a = At(-half.x, .0f, -half.y);
  const auto b = At(half.x, .0f, -half.y);
  const auto d = At(half.x, .0f, half.y);
  const auto e = At(-half.x, .0f, half.y);
  const auto ridgeA = At(-half.x, height, .0f);
  const auto ridgeB = At(half.x, height, .0f);
  const auto sideA = At(.0f, .0f, -1.f) - centre;
  const auto sideB = At(.0f, .0f, 1.f) - centre;
  const auto endA = At(-1.f, .0f, .0f) - centre;
  const auto endB = At(1.f, .0f, .0f) - centre;
  // The two slopes, each a quad from the eave to the ridge, and the two gable ends.
  OrientedQuad(a, b, ridgeB, ridgeA, sideA + glm::vec3(.0f, half.y, .0f));
  OrientedQuad(e, d, ridgeB, ridgeA, sideB + glm::vec3(.0f, half.y, .0f));
  Oriented(a, e, ridgeA, endA);
  Oriented(b, d, ridgeB, endB);
  OrientedQuad(a, b, d, e, {.0f, -1.f, .0f});
  return *this;
}
auto MeshBuilder::Rock(const glm::vec3 &centre, const glm::vec3 &scale, const float roughness, const uint32_t seed, const int detail) -> MeshBuilder & {
  constexpr auto T = 1.61803399f;
  std::vector<glm::vec3> points{{-1, T, 0}, {1, T, 0}, {-1, -T, 0}, {1, -T, 0}, {0, -1, T}, {0, 1, T}, {0, -1, -T}, {0, 1, -T}, {T, 0, -1}, {T, 0, 1}, {-T, 0, -1}, {-T, 0, 1}};
  std::vector<std::array<uint32_t, 3>> faces{{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
  for (auto &p : points)
    p = glm::normalize(p);
  for (auto round = 0; round < detail; ++round) {
    std::unordered_map<uint64_t, uint32_t> middles;
    const auto Middle = [&](const uint32_t a, const uint32_t b) {
      const auto key = static_cast<uint64_t>(std::min(a, b)) << 32u | std::max(a, b);
      if (const auto it = middles.find(key); it != middles.end())
        return it->second;
      points.push_back(glm::normalize(points[a] + points[b]));
      return middles[key] = static_cast<uint32_t>(points.size() - 1);
    };
    std::vector<std::array<uint32_t, 3>> finer;
    for (const auto &f : faces) {
      const auto ab = Middle(f[0], f[1]);
      const auto bc = Middle(f[1], f[2]);
      const auto ca = Middle(f[2], f[0]);
      finer.push_back({f[0], ab, ca});
      finer.push_back({f[1], bc, ab});
      finer.push_back({f[2], ca, bc});
      finer.push_back({ab, bc, ca});
    }
    faces = std::move(finer);
  }
  // Each corner moved once, by its own index, so the faces that share it still meet.
  Random random(seed, 0x510e527fade682d1ull);
  for (auto &p : points)
    p = centre + p * scale * (1.f + random.Range(-roughness, roughness));
  for (const auto &f : faces)
    Oriented(points[f[0]], points[f[1]], points[f[2]], (points[f[0]] + points[f[1]] + points[f[2]]) / 3.f - centre);
  return *this;
}
auto MeshBuilder::SetTransform(const glm::mat4 &value) -> MeshBuilder & {
  transform = value;
  transformed = value != glm::mat4(1.f);
  return *this;
}
auto MeshBuilder::ResetTransform() -> MeshBuilder & {
  return SetTransform(glm::mat4(1.f));
}
auto MeshBuilder::Append(const Mesh &other) -> MeshBuilder & {
  const auto At = [&](const size_t i) { return other.vertices[other.indices.empty() ? i : other.indices[i]].position; };
  const auto count = other.indices.empty() ? other.vertices.size() : other.indices.size();
  for (size_t i = 0; i + 2 < count; i += 3)
    Emit(At(i), At(i + 1), At(i + 2));
  return *this;
}
auto MeshBuilder::IsEmpty() const -> bool {
  return mesh.vertices.empty();
}
auto MeshBuilder::VertexCount() const -> size_t {
  return mesh.vertices.size();
}
auto MeshBuilder::Bounds() const -> BoundingBox {
  return BoundingBox::Calculate(mesh.vertices);
}
auto MeshBuilder::GetMesh() const -> const Mesh & {
  return mesh;
}
auto MeshBuilder::Take() -> Mesh {
  auto taken = std::move(mesh);
  mesh = {};
  return taken;
}
} // namespace kuki
