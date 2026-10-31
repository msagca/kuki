#pragma once
#include <id.hpp>
namespace kuki {
/// @brief A material asset as the asset manager's prefab of it holds it. A drawn entity's material
/// is its pool's (`PoolKey`), not a handle of its own.
struct MaterialHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
