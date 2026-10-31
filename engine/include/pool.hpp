#pragma once
#include <bounding_box.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <hash_utils.hpp>
#include <id.hpp>
#include <limits>
#include <optional>
#include <uuid.hpp>
namespace kuki {
/// @brief What a pool's entities are drawn with: one mesh and one material, shared by all of them.
///
/// A mesh is a mesh asset, or one of a model's meshes when `meshIndex` names it; a material the same,
/// with `materialIndex`. A pool with no mesh is the archetype's own table, whose entities are not
/// drawn -- the place an entity of a drawn archetype is made before it is given a mesh.
struct PoolKey {
  static constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();
  AssetID mesh{};
  uint32_t meshIndex{NONE};
  AssetID material{};
  uint32_t materialIndex{NONE};
  auto operator==(const PoolKey &) const -> bool = default;
  explicit operator bool() const {
    return static_cast<bool>(mesh);
  }
};
/// @brief What a pool is made with besides its key. Given when it is made, and kept.
struct PoolSettings {
  /// @brief The bounds of what the pool draws, around each entity's origin: what culling and the
  /// sun's shadow are fitted by. Left unset, they are the mesh's own. Set empty, the pool is never
  /// culled and is left out of the shadow's fit, which is what a sea reaching the horizon wants.
  std::optional<BoundingBox> bounds{};
  /// @brief Rows to make room for, so that a pool filled when its scene is built is not grown row by
  /// row. A pool may still grow past it.
  size_t capacity{};
};
/// @brief The tag of an archetype whose entities are drawn, and so live in pools.
///
/// What `MeshHandle`, `MaterialHandle`, their model counterparts, a `BoundingBox` and the backends'
/// `RenderBinding` were on every drawn entity: the mesh and material are the pool's, as are the
/// bounds, and a backend keeps what it resolved them to per pool rather than per entity. That is
/// what lets a renderer draw a pool without looking at its entities one by one (see `ArchetypeTable`).
struct Pooled {};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::PoolKey> {
  auto operator()(const kuki::PoolKey &key) const noexcept -> size_t {
    auto seed = hash<kuki::AssetID>{}(key.mesh);
    kuki::hash_combine(seed, hash<uint32_t>{}(key.meshIndex));
    kuki::hash_combine(seed, hash<kuki::AssetID>{}(key.material));
    kuki::hash_combine(seed, hash<uint32_t>{}(key.materialIndex));
    return seed;
  }
};
} // namespace std
