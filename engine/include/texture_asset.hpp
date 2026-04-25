#pragma once
#include <asset.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <texture.hpp>
namespace kuki {
struct KUKI_ENGINE_API TextureAsset final : public Asset {
  TextureAsset(AssetID, std::string = "");
  Texture texture;
};
} // namespace kuki
