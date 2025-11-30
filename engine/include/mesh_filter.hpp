#pragma once
#include <component.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
namespace kuki {
struct KUKI_ENGINE_API MeshFilter final : public IComponent {
  MeshFilter();
  Mesh mesh{};
};
} // namespace kuki
