#pragma once
#include <bounding_box.hpp>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
#include <span>
namespace kuki {
/// @brief Puts a low-poly mesh together a face at a time, each face flat-shaded.
///
/// Every triangle gets vertices of its own, carrying the triangle's own normal, so no two faces
/// share a vertex and every edge is a crease. That is the whole of the low-poly look -- a sphere of
/// twenty faces reads as a cut stone rather than a badly tessellated ball -- and it costs three
/// vertices a triangle where a smooth mesh would share them, which at this many faces is nothing.
///
/// A face's front is the side its normal points to, worked out from its winding. The `Oriented`
/// forms take a direction the face should look towards instead and wind it to suit, which is what
/// a mesh built from polygons of a map wants: the map's own winding depends on which way its plane
/// was laid down, and a face built the wrong way round is culled.
///
/// Everything goes through the current transform, so a part can be built about its own origin and
/// placed with `SetTransform` -- the leaf on the branch, the roof on the house.
class KUKI_ENGINE_API MeshBuilder {
public:
  /// @brief A triangle whose front is the side from which `a`, `b`, `c` go counter-clockwise.
  auto Triangle(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) -> MeshBuilder &;
  /// @brief A triangle turned to face `towards`, however its corners are listed.
  auto Oriented(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &towards) -> MeshBuilder &;
  /// @brief Two triangles, `a b c` and `a c d`, faced as `Triangle` faces them.
  auto Quad(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d) -> MeshBuilder &;
  auto OrientedQuad(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d, const glm::vec3 &towards) -> MeshBuilder &;
  /// @brief A closed ring of points joined to a point in its middle: a polygon's top, or a cone's
  /// sides when the middle stands above the ring. Each triangle faces away from `towards`'s
  /// opposite -- see `Oriented` -- so pass the polygon's up.
  auto Fan(const glm::vec3 &middle, const std::span<const glm::vec3> ring, const glm::vec3 &towards) -> MeshBuilder &;
  /// @brief A solid of `sides` faces round the Y axis, from `bottom` to `top` high, `lower` across
  /// at its foot and `upper` at its head; capped at both ends. An upper radius of nought is a cone,
  /// and three sides a pyramid that looks like it was cut with a knife.
  auto Frustum(const glm::vec3 &base, const float lower, const float upper, const float height, const int sides, const float turn = .0f) -> MeshBuilder &;
  /// @brief An axis-aligned box.
  auto Box(const glm::vec3 &low, const glm::vec3 &high) -> MeshBuilder &;
  /// @brief A box tilted to its own axes: a centre, half its size, and a turn about Y in radians.
  auto Block(const glm::vec3 &centre, const glm::vec3 &half, const float turn = .0f) -> MeshBuilder &;
  /// @brief A roof: a prism with a triangle for a cross-section over a footprint of `half` its size,
  /// its ridge along the local X axis, turned by `turn` about Y, `height` above its eaves.
  auto Gable(const glm::vec3 &centre, const glm::vec2 &half, const float height, const float turn = .0f) -> MeshBuilder &;
  /// @brief A stone: an icosahedron, split `detail` times, each corner pushed in or out by up to
  /// `roughness` of the radius and the whole squashed by `scale`. Seeded, so a stone can be made
  /// again the same.
  auto Rock(const glm::vec3 &centre, const glm::vec3 &scale, const float roughness, const uint32_t seed, const int detail = 0) -> MeshBuilder &;
  /// @brief Applied to everything added after it, until it is set again.
  auto SetTransform(const glm::mat4 &) -> MeshBuilder &;
  auto ResetTransform() -> MeshBuilder &;
  /// @brief Everything another mesh holds, through the current transform.
  auto Append(const Mesh &) -> MeshBuilder &;
  auto IsEmpty() const -> bool;
  auto VertexCount() const -> size_t;
  auto Bounds() const -> BoundingBox;
  /// @brief The mesh as built. The builder keeps it, so it can go on being added to.
  auto GetMesh() const -> const Mesh &;
  /// @brief Hands the mesh over and starts again from nothing.
  auto Take() -> Mesh;
private:
  Mesh mesh;
  glm::mat4 transform{1.f};
  bool transformed{};
  auto Emit(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) -> void;
};
} // namespace kuki
