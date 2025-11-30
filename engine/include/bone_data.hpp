#pragma once
#include <component.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API BoneData final : public IComponent {
  BoneData();
  int boneSSBO{};
  int boneCount{};
};
} // namespace kuki
