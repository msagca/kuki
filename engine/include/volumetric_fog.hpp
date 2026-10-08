#pragma once
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
namespace kuki {
/// @brief Air the scene stands in, which light scatters out of on its way to the camera.
///
/// What this buys is the shaft: a beam of light is invisible from the side unless there is
/// something in the way to scatter it, and until this existed there was nothing in the way. Every
/// light in this engine arrived at a surface, lit it, and left the space between untouched.
///
/// Marched from the camera through the scene's own depth, sampling each light's shadow map at every
/// step, so a shaft is shaped by exactly what the shadow pass already worked out. That is the whole
/// trick and it is why this costs so little: the expensive question -- what can this light see --
/// was answered before the frame started shading anything.
///
/// Unlike `VolumetricClouds` this needs no atmosphere and describes none. The medium here is a
/// height fog with a density and a colour, which is a scene's air rather than a planet's: a sealed
/// room has air in it, and a spot light in a sealed room throws a cone that this is what draws.
/// Where a scene does carry an `AtmosphereSky`, `aerialPerspective` below hands the distance haze
/// over to that component's medium instead, so the two do not both describe the same air.
///
/// A scene singleton and serialized, on the same footing as `AtmosphereSky` and for the same
/// reason: how thick a room's air is describes the work rather than how somebody is looking at it.
struct VolumetricFog {
  /// @brief Extinction of the medium at `baseHeight`, per unit of scene distance.
  ///
  /// The scene's units this time, not the atmosphere's -- everything in this component is measured
  /// against the geometry it sits in rather than against a planet. The default is thin enough to
  /// read as air rather than smoke across a room a few units across, which is what the scenes here
  /// are.
  float density{.06f};
  /// @brief Height at which the density above is the density stated, in scene units.
  float baseHeight{.0f};
  /// @brief How quickly the air thins going up, per unit. Zero fills the scene evenly.
  ///
  /// The reason a shaft is brightest near the floor. An even medium gives a cone of uniform
  /// brightness, which reads as a solid object; thinning it with height puts the haze where dust
  /// settles and lets the beam fade out along its length.
  float heightFalloff{.35f};
  /// @brief What fraction of what the medium removes it scatters rather than absorbs, per channel.
  ///
  /// The fog's colour, stated as an albedo so that it cannot brighten what is behind it: a medium
  /// scatters at most everything it takes out, and the values here are that fraction. Slightly blue
  /// by default, which is what a real haze does and what keeps a white shaft from reading as smoke.
  glm::vec3 albedo{.86f, .9f, 1.f};
  /// @brief How strongly the medium scatters forward, from even to a beam.
  ///
  /// Modest on purpose. A high value here makes every shaft visible only when looking nearly into
  /// its light, which is correct for genuine haze and disappointing in a room, where the shaft is
  /// meant to be seen from the side.
  float anisotropy{.35f};
  /// @brief Light arriving at the medium from everywhere rather than from a light, per channel.
  ///
  /// Without it the fog is black wherever no light reaches it, so a room with one lamp gets a bright
  /// cone standing in a void rather than a bright cone standing in air. Gathered with no phase term,
  /// which is what "from everywhere" means -- the isotropic phase integrates to one over the sphere.
  glm::vec3 ambient{.16f, .18f, .22f};
  /// @brief What the in-scattered light from the scene's lights is multiplied by.
  ///
  /// A viewing decision rather than a fact about the air, like `AtmosphereSky::sunDiscIntensity`:
  /// the physically right shaft is often a faint one, and a scene that wants the beam to be the
  /// subject needs to be able to say so without lying about the density.
  float lightScale{1.f};
  /// @brief How far from the camera the march goes, in scene units.
  ///
  /// Beyond this the medium is taken to continue unchanged, so a distant surface still fogs rather
  /// than snapping clear -- what is lost past here is the shadowing, not the haze. Kept short
  /// because the step count is fixed: doubling the distance halves the resolution of every shaft in
  /// the frame.
  float maxDistance{60.f};
  /// @brief How much of the atmosphere's own distance haze is applied, where a scene has one.
  ///
  /// Nothing to do with the medium above. A scene carrying an `AtmosphereSky` has air described in
  /// kilometres of Rayleigh and Mie, and a surface a long way off is seen through it -- which is
  /// what turns a far hillside blue and is the only reason distance reads as distance at all.
  /// This applies that, using the same figures the sky was marched from, so the haze in front of a
  /// mountain and the sky behind it are the same air.
  ///
  /// Does nothing in a scene with no atmosphere, which is not a failure: a sealed room has no
  /// aerial perspective to apply, and the height fog above is the whole of its air.
  float aerialPerspective{1.f};
  /// @brief How many kilometres one unit of the scene is, for the haze above and nothing else.
  ///
  /// `AtmosphereSky` says plainly that none of it has to agree with the scene's units, and that is
  /// true of a sky: it is a background at infinity, so no distance in it is a distance to anything
  /// you could stand next to. Aerial perspective is the one thing that breaks that, because it is
  /// the air in front of something the scene does contain, and how blue a far wall goes depends on
  /// how far away the wall really is.
  ///
  /// So the conversion is asked for here, where it is needed, rather than added to the atmosphere,
  /// where it would imply the rest of that component depended on it. The default puts a sixty unit
  /// draw distance at about a fifth of a kilometre, which is a large room or a small courtyard --
  /// raise it for a scene meant to read as a landscape, where a hillside at the far plane should be
  /// kilometres off and hazed accordingly.
  float kilometresPerUnit{.0035f};
  /// @brief Steps the view march takes between the camera and the scene.
  ///
  /// Per pixel per frame, and each one samples a shadow map, so this is the cost of the pass almost
  /// entirely. Marched at the viewport's own resolution rather than at half it, unlike the clouds:
  /// the samples are coherent between neighbouring pixels -- they walk the same shadow map through
  /// nearly the same texels -- so they are far cheaper than the count suggests, and a shaft cutting
  /// across a silhouette is exactly where a half resolution gather shows its seams.
  ///
  /// The starting offset is dithered per pixel, so what too few steps costs is a fine grain rather
  /// than visible bands. Thirty-two is where that grain stops being noticeable on a shaft seen from
  /// the side, which is the hardest case.
  uint32_t steps{32};
  auto operator==(const VolumetricFog &) const -> bool = default;
};
} // namespace kuki
