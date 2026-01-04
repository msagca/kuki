#pragma once
#include <asset.hpp>
#include <glm/ext/vector_float4.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
namespace kuki {
struct KUKI_ENGINE_API MaterialAsset final : public Asset {
  MaterialAsset(const AssetID = AssetID::Invalid);
  std::vector<size_t> textures{};
  MaterialFallback fallback{};
};
} // namespace kuki
