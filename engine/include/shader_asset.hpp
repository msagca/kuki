#pragma once
#include <asset.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <string>
namespace kuki {
struct KUKI_ENGINE_API ShaderAsset final : public Asset {
  ShaderAsset(const AssetID = AssetID::Invalid, std::string = "");
  MaterialType type{MaterialType::Unknown};
  std::string text{};
  EntityID resourceId{};
};
} // namespace kuki
