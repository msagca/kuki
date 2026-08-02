#pragma once
#include <color.hpp>
#include <texture_content.hpp>
#include <variant>
#include <vector>
namespace kuki {
struct Texture {
  ColorRange range{ColorRange::LDR};
  ColorSpace color{ColorSpace::sRGB};
  TextureContent content{TextureContent::Albedo};
  int channels{3};
  int height{1024};
  int width{1024};
  bool flipY{false};
  std::variant<std::vector<unsigned char>, std::vector<float>> data;
};
} // namespace kuki
