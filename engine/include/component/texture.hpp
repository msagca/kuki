#pragma once
#include "component.hpp"
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API Texture final : public IComponent {
  Texture()
    : IComponent(std::in_place_type<Texture>) {}
  TextureType type{};
  int width{};
  int height{};
  int id{};
  void CopyTo(Texture&) const;
};
} // namespace kuki
