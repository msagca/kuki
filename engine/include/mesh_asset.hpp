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
  MeshAsset(AssetID, std::string = "");
  Mesh mesh;
  AssetID material{};
  BoundingBox bounds{};
  EntityID resourceId{};
};
} // namespace kuki
