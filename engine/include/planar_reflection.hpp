#pragma once
namespace kuki {
/// @brief A level surface that mirrors what stands on it: a street after rain, a polished floor, a
/// pond. Reflected in screen space, so it mirrors only what is on the screen.
///
/// One horizontal plane and not every glossy surface, which is what makes it cheap enough to leave
/// on. A plane's normal is known without being stored, so the pass needs nothing the frame does not
/// already have -- the picture and its depth -- and finds the plane's pixels by the height the depth
/// puts them at. A general screen-space reflection would need a buffer of normals the scene pass
/// does not write.
///
/// The surface is not uniformly wet. Puddles are patches of noise over the plane, mirror-like
/// inside and reflecting only `dryReflectance` of that outside, and each is broken by rings where
/// rain lands in it, for as long as `ripples` says it is raining.
///
/// What leaves the screen is not reflected: a ray that walks off the top of the picture finds
/// nothing and fades out rather than inventing a sky. That limit is what screen space costs, and
/// a stage is arranged so the street mirrors the buildings in front of the camera, not the sky
/// behind it.
///
/// A scene singleton, and saved: how wet the ground is belongs to the scene.
struct PlanarReflection {
  /// @brief Height of the plane, in scene units. Pixels within `tolerance` of it reflect.
  float height{.0f};
  float tolerance{.02f};
  /// @brief How much of the reflection reaches the picture inside a puddle, before the Fresnel term.
  /// Nought switches the pass off and costs it a copy.
  float strength{.0f};
  /// @brief How much of the puddles' reflection the rest of the wet surface shows.
  float dryReflectance{.25f};
  /// @brief How much of the plane is puddle, from nought to one, and how large the patches are --
  /// patches a unit, about.
  float puddleCoverage{.5f};
  float puddleScale{.35f};
  /// @brief How blurred the reflection is, from mirror at nought to a smear at one.
  float blur{.25f};
  /// @brief How hard the rain disturbs the puddles, and how many places it lands per unit.
  float ripples{.0f};
  float rippleDensity{2.5f};
  /// @brief How far a reflected ray is followed before it is given up, in scene units.
  float maxDistance{30.f};
  auto operator==(const PlanarReflection &) const -> bool = default;
};
} // namespace kuki
