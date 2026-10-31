#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
#include <primitive.hpp>
#include <string>
namespace kuki {
struct KUKI_ENGINE_API MeshAsset final : public Asset {
  MeshAsset(AssetID);
  Mesh mesh;
  AssetID material{};
  BoundingBox bounds{};
  /// @brief Raised by whoever changes `mesh` after it has been drawn, for the change to be seen.
  ///
  /// A backend uploads a mesh once and draws that copy from then on, so a mesh built by a game and
  /// rebuilt as the game goes -- a highlight under the cursor, a path, an overlay of the map -- would
  /// otherwise go on being drawn as it first was. Raising this makes the Direct3D 12 backend upload
  /// it again, and let the old copy go once the frames drawing it are done.
  uint32_t revision{};
};
} // namespace kuki
