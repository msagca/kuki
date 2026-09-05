#pragma once
namespace kuki {
/// @brief What the running backend can actually do, so nothing is offered that it cannot.
///
/// The two backends are not ports of one another and were never going to be. The Direct3D 12 one
/// traces rays into a probe field; the OpenGL one has an image-based indirect term and no field at
/// all. That difference is fine. What was not fine is that nothing above the renderer knew about
/// it: the editor drew the same sixteen indirect-lighting sliders, the same fifteen debug views and
/// the same probe visualisations whichever backend was running, and under OpenGL every one of them
/// did nothing at all. A control that does nothing is worse than an absent one, because the person
/// using it concludes the renderer is broken rather than that the feature is elsewhere.
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
  /// probe anywhere. `scene.hlsl` branches on the selected view and `lit.frag` does not, so today
  /// this tracks the same backend as the field above, and it is asked as its own question because
  /// the answer will not always be the same one.
  bool lightingDebugViews{};
};
} // namespace kuki
