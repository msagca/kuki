#pragma once
namespace kuki {
/// @brief What the running backend can actually do, so nothing is offered that it cannot.
///
/// Backends are not ports of one another and are never going to be. When there were two, the
/// Direct3D 12 one traced rays into a probe field and the OpenGL one had an image-based indirect
/// term and no field at all. That difference was fine. What was not fine is that nothing above the
/// renderer knew about it: the editor drew the same sixteen indirect-lighting sliders, the same
/// fifteen debug views and the same probe visualisations whichever backend was running, and under
/// OpenGL every one of them did nothing at all. OpenGL is gone; the next backend will differ too.
/// A control that does nothing is worse than an absent one, because the person using it concludes
/// the renderer is broken rather than that the feature is elsewhere.
///
/// So a backend says what it has and the editor asks. Read once when the editor starts rather than
/// polled: a backend cannot be swapped without restarting the process, which is what the graphics
/// panel says when you pick a different one.
///
/// Every field defaults to false and the base `Renderer` returns the struct as it stands, so a
/// backend that says nothing is taken to have none of this. That is the safe direction -- a new
/// capability appearing in this struct hides its controls everywhere until a backend claims it,
/// where the other default would silently offer them everywhere until each backend denied it.
struct RendererCapabilities {
  /// @brief Whether there is an irradiance probe field behind the indirect diffuse term.
  ///
  /// Gates most of the `IndirectLighting` panel and all of the probe visualisations. Not a
  /// statement about the API but about this machine: the Direct3D 12 backend needs bindless
  /// descriptors and inline raytracing to fill the field, and reports false on hardware providing
  /// neither, which is the same thing the context already warns about at startup. A probe volume
  /// whose passes are being skipped should not be offering knobs either.
  bool probeVolume{};
  /// @brief Whether the scene shader can write a step of its shading out in place of the pixel.
  ///
  /// Separate from the field because it is a property of the shader rather than of what feeds it: a
  /// backend could perfectly well show its direct light and its occlusion term without having a
  /// probe anywhere. `scene.hlsl` branches on the selected view, so today this tracks the same
  /// backend as the field above, and it is asked as its own question because the answer will not
  /// always be the same one.
  bool lightingDebugViews{};
  /// @brief Whether the backend can compute a sky from an atmosphere instead of sampling one.
  ///
  /// Gates the whole `AtmosphereSky` panel and its entry in the add-component menu. A component the
  /// running backend cannot act on is worse than an absent one: it would sit in the hierarchy
  /// looking like the scene had a sky, while the picture showed the flat background.
  ///
  /// Not a statement about the hardware, unlike `probeVolume`. The march is plain Shader Model 6.0
  /// compute writing a cubemap the backend already keeps, so a backend either has the path or does
  /// not -- and the OpenGL one did not, because its environment chain was fed from a texture asset
  /// and there is no texture here.
  bool proceduralSky{};
  /// @brief Whether the backend marches the air: the cloud layer, the shafts and the distance haze.
  ///
  /// One flag for both passes rather than two, because they are one piece of work seen from either
  /// end -- the fog pass reads the cloud pass's buffer so that a shaft is broken by the cloud in
  /// front of it, and a backend with one and not the other would draw a beam straight through an
  /// overcast sky. Nothing would be gained by letting a backend claim half of that.
  ///
  /// Gates both panels and both entries in the add-component menu. Unlike `proceduralSky` the fog
  /// half needs no atmosphere and works in a sealed room off a spot light's shadow map, so this is
  /// not implied by that flag and is asked separately.
  bool volumetrics{};
  /// @brief Whether the backend simulates and draws particle emitters.
  ///
  /// Gates the `ParticleEmitter` panel and its entry in the add-component menu, for the reason the
  /// sky's flag does: an emitter the backend cannot run would sit in the hierarchy looking like the
  /// scene had sparks in it while the picture showed none.
  ///
  /// Not a statement about the hardware. The simulation is one Shader Model 6.0 compute dispatch
  /// over a structured buffer and the draw is an instanced quad, which any modern API has every
  /// piece of. A backend without it lacks the pass -- as OpenGL's did -- not the means to write it.
  bool particles{};
  /// @brief Whether the backend draws a scene as a cel: bands on the materials that ask for them,
  /// ink along the outlines, and the grade and screen effects over the finished picture.
  ///
  /// One flag for all of it because it is one look, and half of it is a different look rather than
  /// a lesser one -- bands with no ink read as a flat render, not as a drawing. Gates the
  /// `Stylization` panel, the material's toon slider, and the add menu's entry.
  bool stylization{};
  /// @brief Whether the backend moves surfaces by the scene's `Wind`: the materials' sway and waves.
  ///
  /// Gates the `Wind` panel and its entry in the add menu. A backend without it draws every surface
  /// where its mesh puts it, which is a still day rather than a wrong picture.
  bool wind{};
};
} // namespace kuki
