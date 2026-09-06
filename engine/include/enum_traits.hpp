#pragma once
#include <camera_type.hpp>
#include <debug_view.hpp>
#include <exposure.hpp>
#include <light_type.hpp>
#include <material_type.hpp>
#include <render_pass.hpp>
#include <tone_mapper.hpp>
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
struct EnumTraits<LightingDebugView> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"None", "Indirect Diffuse", "Sky Irradiance", "Direct Light", "Surface Occlusion", "Probe Visibility", "Probe Weight", "Probe Fallback", "Probe Cell", "Probe Blend"};
    return names;
  }
};
template <>
struct EnumTraits<ProbeDebugView> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Off", "Irradiance", "Distance", "Variance", "Trust", "Relocation", "Classification"};
    return names;
  }
};
template <>
struct EnumTraits<ExposureMode> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Manual", "Physical"};
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
template <>
struct EnumTraits<AlphaMode> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Opaque", "Mask", "Blend"};
    return names;
  }
};
template <>
struct EnumTraits<MaterialType> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"Lit", "Unlit"};
    return names;
  }
};
template <>
struct EnumTraits<RenderPass> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"AntiAliasing", "BloomEffect", "BlurEffect", "BrightPassFilter", "DepthPrepass", "Outline", "Overlay", "ProbeTrace", "Scene", "ShadowMap", "SpotShadowMap", "ToneMapping"};
    return names;
  }
};
template <>
struct EnumTraits<ToneMapper> {
  static const std::vector<const char *> &GetNames() {
    static const std::vector<const char *> names = {"None", "Reinhard", "ACES", "AgX"};
    return names;
  }
};
} // namespace kuki
