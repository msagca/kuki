#pragma once
#include <asset.hpp>
#include <color_space.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <texture.hpp>
#include <texture_content.hpp>
#include <texture_type.hpp>
namespace kuki {
struct KUKI_ENGINE_API TextureAsset final : public Asset {
  TextureAsset(AssetID, std::string = "");
  Texture texture;
  EntityID resourceId{};
};
} // namespace kuki
