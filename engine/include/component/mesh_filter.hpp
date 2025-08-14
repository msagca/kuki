#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API MeshFilter final : public IComponent {
  MeshFilter()
    : IComponent(std::in_place_type<MeshFilter>) {}
  Mesh mesh{};
};
} // namespace kuki
