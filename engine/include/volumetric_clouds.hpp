#pragma once
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
namespace kuki {
/// @brief A layer of cloud in the atmosphere, marched rather than drawn.
///
/// Everything here is a property of that layer, and the altitudes are in the same kilometres
/// `AtmosphereSky` is written in, for the reason that component gives: the sky is a background at
/// infinity and has no conversion factor to the scene's units. A cloud base at one and a half is a
/// cloud base at fifteen hundred metres above the ground, whatever a unit means down in the scene.
///
/// Requires an `AtmosphereSky` on the same entity and does nothing without one. Not a limitation
/// worth working around: a cloud is lit by the sun that has already been filtered through the air
/// above it and filled in by the sky around it, and both of those are integrals through a medium
/// only the atmosphere describes. A cloud layer with no atmosphere would have to invent a sun
/// colour and an ambient term, and the two inventions would disagree with the sky behind them.
///
/// Marched every frame rather than baked into the sky cubemap with the rest of the atmosphere, and
/// that is the one decision here that costs anything. Baked clouds are nearly free -- the cubemap
/// is rebuilt only when the sun moves -- and they would light the scene through the existing
/// image-based chain without another line of code. They would also be nailed to the sky: no drift,
/// and no parallax as the camera moves, which is most of what tells the eye a cloud is a volume
/// rather than a photograph of one. So they are marched from the camera, and what the scene loses
/// -- ambient light that responds to cloud cover -- is bought back by `shadowStrength` below.
///
/// @see `VolumetricFog`, which marches the air between the camera and the scene, and reads this
/// layer's shadow so that a shaft is broken by the cloud that should have broken it.
struct VolumetricClouds {
  /// @brief Altitude of the cloud base above the ground, in kilometres.
  float bottomAltitude{1.5f};
  /// @brief Altitude of the cloud top. The layer is the gap between the two.
  ///
  /// Two and a half kilometres of it by default, which is a fair weather cumulus field. A thin
  /// layer reads as stratus however the shape is tuned, because there is no room in it for the
  /// vertical development that makes a cumulus look like one.
  float topAltitude{4.f};
  /// @brief How much of the sky has cloud in it, from a clear sky to an overcast one.
  ///
  /// Threshold rather than a multiplier: it is subtracted from the shape noise before the result is
  /// used as density, so raising it grows existing clouds together rather than making every cloud
  /// uniformly thicker. That is what cloud cover actually does, and it is why this and `density`
  /// are two knobs rather than one.
  float coverage{.45f};
  /// @brief Extinction of the cloud interior, per kilometre.
  ///
  /// Large compared to anything in `AtmosphereSky`, and it should be: a cloud is optically thick
  /// across a few hundred metres where the clear air above it is not thick across a hundred
  /// kilometres. This is the number that decides whether a cloud is a solid white wall or something
  /// the sun comes through.
  float density{12.f};
  /// @brief Direction the layer drifts in, in the horizontal plane. Normalised on use.
  glm::vec2 windDirection{1.f, .35f};
  /// @brief How fast it drifts, in kilometres per second of wall clock.
  ///
  /// Small numbers. A cloud field crossing a kilometre a minute is already brisk to look at, and
  /// anything above about a tenth reads as weather on fast forward rather than as wind.
  float windSpeed{.012f};
  /// @brief Size of the largest features, in kilometres.
  ///
  /// The wavelength of the shape noise, so it sets how big one cloud is. Raising it makes fewer,
  /// larger clouds out of the same field rather than spreading the ones already there.
  float shapeScale{8.f};
  /// @brief Size of the features the detail noise erodes the edges with, in kilometres.
  float detailScale{.6f};
  /// @brief How deeply that erosion cuts, from a smooth edge to a shredded one.
  ///
  /// Applied to the cloud's boundary rather than its interior, which is what makes it cheap to look
  /// right: the inside of a cloud is opaque and nothing there survives to be seen, so detail spent
  /// on it is detail thrown away.
  float detailStrength{.35f};
  /// @brief How strongly the droplets scatter forward, from even to a beam.
  ///
  /// Higher than the aerosol figure in `AtmosphereSky` and for the same physical reason carried
  /// further: cloud droplets are enormous compared to a wavelength, so almost everything goes
  /// forward. This is what puts the silver lining on a cloud with the sun behind it.
  float anisotropy{.72f};
  /// @brief How much of the backward lobe is kept alongside the forward one.
  ///
  /// A cloud lit from behind has a bright rim, and a cloud lit from in front is bright all over;
  /// one Henyey-Greenstein lobe can say the first or the second and not both. This mixes a weak
  /// backward lobe in, which is the cheap half of what a Mie phase function would give.
  float backscatter{.25f};
  /// @brief How dark the approach to a cloud's edge goes before it brightens.
  ///
  /// The powder term. Light entering a cloud has to scatter several times before it comes back out,
  /// so a thin edge seen against the sun is darker than the single-scattering estimate says -- the
  /// effect that makes the underside of a cumulus look bruised rather than grey. Zero removes it,
  /// which is worth doing once to see what it was doing.
  float powder{.6f};
  /// @brief What the sky around a cloud fills its shaded side with, as a fraction of that sky.
  ///
  /// Sampled from the same cubemap the scene's image-based lighting reads, so a cloud under a blue
  /// sky is filled with blue and one at sunset is filled with orange, without either being stated
  /// here.
  float ambient{.9f};
  /// @brief What the whole layer's scattering is multiplied by before anything else sees it.
  ///
  /// Here for the reason `AtmosphereSky::sunIntensity` is: the engine's lights carry bare
  /// multipliers rather than photometric units, so there is no absolute brightness to be correct
  /// about. One is the value the defaults were tuned at.
  float scatteringScale{1.f};
  /// @brief How much the layer darkens the light reaching the air beneath it, from none to all.
  ///
  /// The one place this component reaches out of the sky. A marched cloud is not in the shadow map
  /// -- it is not geometry and there is nothing to rasterise -- so nothing below it knows it is
  /// there. What this reaches is `VolumetricFog`: the sun's contribution to every shaft is scaled
  /// by how much of it survived this layer, so a cloud drifting across the sun takes the beams with
  /// it. Costs one texel, computed once a frame.
  ///
  /// What it does not yet reach is surface shading. The scene is shaded before the cloud pass runs
  /// and reads nothing from it, so the ground under an overcast sky is lit as though it were clear.
  /// Closing that means the scene pass sampling a cloud shadow map of its own, which is a larger
  /// change than this one.
  float shadowStrength{.7f};
  /// @brief Steps the view march takes through the layer.
  ///
  /// The cost control, and the one that matters: this is per pixel per frame, unlike everything in
  /// `AtmosphereSky`, which is per cubemap texel and only when the sun moves. The march is
  /// distributed over the layer's thickness rather than over the whole ray, so a low count shows
  /// first as banding in the thick parts of a cloud.
  uint32_t steps{64};
  /// @brief Steps the march towards the sun takes from each of those, to find the cloud's own shadow.
  ///
  /// Multiplies the count above, so it is kept small. Six is enough because what it is integrating
  /// is smooth: the sun ray leaves the cloud quickly in every direction that matters, and the
  /// several-orders-of-scattering term is what covers for the rest.
  uint32_t lightSteps{6};
  auto operator==(const VolumetricClouds &) const -> bool = default;
};
} // namespace kuki
