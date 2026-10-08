#pragma once
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
namespace kuki {
/// @brief A sky computed from the air it passes through, rather than sampled from a photograph.
///
/// Every value here is a property of an atmosphere, and the defaults are Earth's. They are in
/// kilometres and reciprocal kilometres, which is the only place in this engine that is true and is
/// worth being clear about: none of it has to agree with the scene's units. The sky is a background
/// at infinity, so the planet's radius is not a distance to anything in the scene, and the one
/// field that relates the two is `viewAltitude` -- how high up the scene is meant to be, stated in
/// the atmosphere's units because there is no conversion factor to read anywhere.
///
/// A component for the reason `IndirectLighting` and [[anti-aliasing]] are, and it sits on the same
/// settings entity. Unlike those two it IS serialized, and the divergence is deliberate: a sample
/// count and a debug view describe how somebody is looking at their work, while "this scene is at
/// eight kilometres under a setting sun" is the work. It travels with the scene the way a light
/// does.
///
/// Present is what switches the procedural sky on. A scene with this component gets its sky from
/// here and ignores any `SkyboxHandle` texture; a scene without one behaves exactly as before.
/// Removing the component is how you go back to the photograph.
///
/// The sun is not here. It is the scene's first directional `Light`, because a scene that has a sun
/// already has one of those and two places to set its direction is one place to get them out of
/// step. What this means in practice is that rotating the light moves the sky, which is the right
/// way round and the opposite of what `RenderingSystem::UpdateSkyboxLight` does for a photographed
/// sky -- that reads a light out of the brightest part of an image, and there is no image here.
struct AtmosphereSky {
  /// @brief Radius of the planet, which is where the ground is and what casts the night side.
  ///
  /// Earth's, near enough. It mostly decides how sharply the horizon curves away, which at these
  /// scales shows up as how quickly the sky darkens with altitude rather than as visible curvature.
  float bottomRadius{6360.f};
  /// @brief Thickness of the atmosphere above the ground. The top radius is the two added.
  float atmosphereHeight{100.f};
  /// @brief Rayleigh scattering coefficient per kilometre, per channel.
  ///
  /// Why the sky is blue: the three differ by about a factor of six across the visible range, so
  /// short wavelengths are scattered out of a sunbeam and into every other direction. Scaling all
  /// three together makes for a thicker or thinner-looking air; changing their ratio is how you get
  /// a sky that is not Earth's.
  glm::vec3 rayleighScattering{5.802e-3f, 13.558e-3f, 33.1e-3f};
  /// @brief Altitude over which the air thins by a factor of e.
  float rayleighScaleHeight{8.f};
  /// @brief Mie scattering coefficient per kilometre, for the aerosol layer.
  ///
  /// Haze. Grey rather than coloured, because the particles are large compared to a wavelength, and
  /// strongly forward-scattering -- which together are what put a bright halo around the sun and
  /// wash the horizon out on a humid day.
  float mieScattering{3.996e-3f};
  /// @brief Mie extinction per kilometre, which is the scattering plus what the aerosols absorb.
  ///
  /// Must not be below `mieScattering`: a medium cannot scatter more light than it removes. Nothing
  /// enforces that, and the symptom of getting it wrong is a haze that brightens what is behind it.
  float mieAbsorption{4.4e-3f};
  float mieScaleHeight{1.2f};
  /// @brief How strongly the aerosol layer scatters forward, from zero for even to one for a beam.
  ///
  /// The knob to reach for when the sun looks like a sticker rather than a light: raising it tightens
  /// the halo and brightens the sky immediately around the sun at the expense of everywhere else.
  float mieAnisotropy{.8f};
  /// @brief Ozone absorption per kilometre, per channel.
  ///
  /// Absorbs and does not scatter, and absorbs most in the middle of the visible range, which is
  /// what takes the green out of a low sun and leaves a sunset red instead of merely dim. Setting
  /// this to zero is the quickest way to see what it is doing.
  glm::vec3 ozoneAbsorption{.650e-3f, 1.881e-3f, .085e-3f};
  /// @brief Altitude the ozone layer is centred on, and how far it reaches either side.
  ///
  /// A layer rather than a gas that thins with height, so it is modelled as a tent that peaks here
  /// and falls to nothing at `ozoneWidth` away in both directions.
  float ozoneCenter{25.f};
  float ozoneWidth{15.f};
  /// @brief How high above the ground the scene sits, in kilometres.
  ///
  /// The field this was built for. At zero the sky is the one seen from a field; at a few kilometres
  /// the horizon drops, the blue deepens overhead and the haze thins, which is what a chessboard up
  /// in the sky should be standing in. Above `atmosphereHeight` there is no air left to scatter and
  /// the sky goes black, which is correct and probably not what was wanted.
  float viewAltitude{2.f};
  /// @brief What the whole sky is multiplied by before anything else sees it.
  ///
  /// Not a physical quantity, and it cannot be: this engine's lights carry bare multipliers rather
  /// than photometric units -- see `ExposureMode::Physical` -- so there is no illuminance for the
  /// sun that would make the sky come out at a defensible absolute brightness. The default is
  /// chosen to land the sky in the same range the analytic gradient it replaces occupied, so a
  /// scene's exposure does not have to move when this component is added.
  float sunIntensity{10.f};
  /// @brief Angular radius of the sun's disc, in degrees.
  ///
  /// The real sun is a quarter of a degree and looks it. Larger reads as a bigger, softer star and
  /// is the honest way to get a hazier sun than the Mie term alone gives; it also costs nothing,
  /// since the disc is a comparison against a cosine rather than anything drawn.
  float sunAngularRadius{.27f};
  /// @brief What the disc's radiance is multiplied by, on top of what the physics gives.
  ///
  /// The physical figure is not in doubt and is not a good default. A disc is the sun's whole
  /// output arriving from a solid angle of about seven millionths of a steradian, which makes it
  /// some four orders of magnitude brighter than the sky beside it -- correct, and more than the
  /// bloom chain can be handed without turning the frame white. One is a value picked by looking,
  /// and it is a viewing decision rather than a fact about the air, which is why it is a multiplier
  /// on the physics rather than a replacement for it.
  ///
  /// Zero removes the disc and leaves the sky it sits in untouched. That is the setting for a scene
  /// whose sun is meant to be somewhere off camera.
  float sunDiscIntensity{1.f};
  /// @brief How much of the multiple-scattering estimate is applied, from none to all of it.
  ///
  /// One is the answer; this exists because the difference is otherwise hard to see and easy to
  /// mistake for something else. At zero the sky is single scattering only: too dark towards the
  /// horizon, and with no blue hour at all once the sun is down. Turning it off and on is the
  /// fastest way to confirm the estimate is doing what it should.
  float multiscatterStrength{1.f};
  /// @brief How much light the ground bounces back up, as a grey Lambertian albedo.
  ///
  /// Most of the lower half of the sky cubemap is ground, and that cubemap is where the diffuse
  /// irradiance and the reflections are gathered from -- so this is not a detail of the background.
  /// It is most of the fill light arriving from below.
  float groundAlbedo{.3f};
  /// @brief Steps the sky march takes along each view ray.
  ///
  /// The cost control. The march runs once per cubemap texel and only when something here changes,
  /// so this buys smoothness in the gradient rather than frame time. Below about sixteen the
  /// banding shows in the sky near the horizon, where the medium changes fastest along a ray.
  uint32_t marchSteps{32};
  auto operator==(const AtmosphereSky &) const -> bool = default;
};
} // namespace kuki
