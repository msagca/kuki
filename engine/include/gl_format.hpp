#pragma once
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
struct GLFormat {
  int external;
  int internal;
};
KUKI_ENGINE_API auto GLFormatToTarget(const unsigned int) -> TargetFormat;
KUKI_ENGINE_API auto TargetFormatToGL(const TargetFormat &) -> GLFormat;
} // namespace kuki
