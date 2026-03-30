#pragma once
#include <color_space.hpp>
#include <texture_content.hpp>
#include <texture_type.hpp>
#include <vector>
namespace kuki {
struct Texture {
  std::vector<unsigned char> data;
  int width{1024};
  int height{1024};
  int channels{3};
  ColorSpace color{ColorSpace::sRGB};
  TextureContent content{TextureContent::Albedo};
  TextureType type{TextureType::UV2D};
};
} // namespace kuki
