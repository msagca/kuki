#pragma once
#include <component.hpp>
#include <kuki_engine_export.h>
#include <material.hpp>
namespace kuki {
struct KUKI_ENGINE_API MeshRenderer final : public IComponent {
  MeshRenderer();
  Material material{};
};
} // namespace kuki
