#pragma once
#include <asset.hpp>
#include <color_space.hpp>
#include <kuki_engine_export.h>
#include <texture_content.hpp>
#include <texture_type.hpp>
namespace kuki {
struct KUKI_ENGINE_API TextureAsset final : public Asset {
  TextureAsset(const AssetID = AssetID::Invalid);
  TextureType type{TextureType::UV2D};
  TextureContent content{TextureContent::Albedo};
  ColorSpace color{ColorSpace::sRGB};
  int width{1024};
  int height{1024};
  int channels{3};
  std::vector<unsigned char> data{};
};
} // namespace kuki
