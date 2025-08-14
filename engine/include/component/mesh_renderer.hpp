#pragma once
#include "component.hpp"
#include "material.hpp"
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API MeshRenderer final : public IComponent {
  MeshRenderer()
    : IComponent(std::in_place_type<MeshRenderer>) {}
  Material material{};
};
} // namespace kuki
