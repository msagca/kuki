#pragma once
#include <asset.hpp>
#include <cstddef>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <texture.hpp>
namespace kuki {
struct KUKI_ENGINE_API TextureAsset final : public Asset {
  TextureAsset(AssetID);
  Texture texture;
  AssetID modelAssetId{};
  size_t modelTextureIndex{};
};
} // namespace kuki
