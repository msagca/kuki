#pragma once
#include <component.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API Texture final : public IComponent {
  Texture();
  TextureType type{};
  int width{};
  int height{};
  int id{};
};
} // namespace kuki
