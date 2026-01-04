#pragma once
#include <cstdint>
namespace kuki {
enum class ComputeType : uint8_t {
  BRDF_LUT,
  CubemapEquirect,
  EquirectCubemap,
  IrradianceMap,
  PrefilterMap,
  Unknown
};
}
