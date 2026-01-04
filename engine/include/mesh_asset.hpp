#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <texture_asset.hpp>
namespace kuki {
struct KUKI_ENGINE_API MeshAsset final : public Asset {
  MeshAsset(const AssetID = AssetID::Invalid);
  std::vector<Vertex> vertices{};
  std::vector<unsigned int> indices{};
  BoundingBox bounds{};
  size_t material{};
};
} // namespace kuki
