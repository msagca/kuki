#pragma once
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <optional>
#include <texture.hpp>
namespace kuki {
/// @brief A directional light inferred from an environment map's brightest region.
struct DominantLight {
  /// @brief Direction the light travels, ready to assign to `Light::forward`.
  ///
  /// This is the negation of the direction towards the bright region, since a light points away
  /// from its source.
  glm::vec3 direction{.0f, -1.f, .0f};
  glm::vec3 color{1.f};
  /// @brief How much brighter the peak is than the average, as a multiplier.
  ///
  /// Reported so callers can scale a light by how confident the extraction was rather than
  /// treating a faint hotspot the same as a sun.
  float contrast{1.f};
};
/// @brief Finds the dominant light direction in an equirectangular environment map.
///
/// Bins the image into a coarse grid and takes the brightest bin rather than the brightest texel,
/// so a single hot pixel or sensor noise cannot swing the result. The bin's centre is converted to
/// a direction using the same mapping as `cubemap_equirect.comp`, which is the project's authority
/// on equirectangular conventions.
///
/// @param minimumContrast How many times brighter than the image average the peak must be before
///        it counts as a light source rather than general sky brightness.
/// @return Nothing when the map has no region meaningfully brighter than the rest, which is the
///         case for a smooth gradient sky. Callers should fall back to a scene light there.
auto KUKI_ENGINE_API ExtractDominantLight(const Texture &, const float = 3.f) -> std::optional<DominantLight>;
} // namespace kuki
