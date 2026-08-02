#pragma once
#include <asset.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
#include <material_type.hpp>
#include <string>
namespace kuki {
struct KUKI_ENGINE_API MaterialAsset final : public Asset {
  MaterialAsset(AssetID);
  MaterialFallback fallback;
  MaterialType type{MaterialType::Unlit};
  std::vector<AssetID> textures;
};
} // namespace kuki
