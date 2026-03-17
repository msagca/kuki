#pragma once
#include <camera_type.hpp>
#include <light_type.hpp>
#include <vector>
namespace kuki {
template <typename T>
struct EnumTraits {
  static_assert(sizeof(T) == 0, "EnumTraits must be specialized for this type.");
  static const std::vector<const char *> &GetNames() {}
};
template <>
struct EnumTraits<CameraType> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Perspective", "Orthographic"};
    return names;
  }
};
template <>
struct EnumTraits<LightType> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Directional", "Point", "Spot"};
    return names;
  }
};
} // namespace kuki
