#pragma once
#include <cstdint>
namespace kuki {
/// @brief Which step of the shading pass is written to the screen in place of the finished pixel.
///
/// The render graph's targets can already be looked at one by one, which covers every step of
/// indirect lighting that ends in a resource: the depth prepass and the shadow maps. What it cannot
/// reach is the larger part. The probe field is located, interpolated, tested
/// for visibility, weighted, crushed and folded into the ambient term inside a single pixel shader,
/// and what leaves that shader is one colour with every one of those decisions already summed into
/// it. An intermediate in a shader can only be inspected if the shader is asked to write it out.
///
/// So these are neither passes nor resources. Each names a value the scene pass already computes,
/// promoted to the output for as long as it is selected. Nothing here changes what the finished
/// image would have been, and nothing here costs a pass, which is what makes it safe to leave in a
/// release build rather than something to be compiled out.
///
/// While one is selected the tone mapping curve is bypassed, so what reaches the screen is the value
/// itself rather than a photograph of it. See `DXPostConstants::raw`.
///
/// The order is load-bearing: `EnumTraits<LightingDebugView>` names these by index and the shaders
/// branch on the same numbering, so inserting one in the middle relabels every entry after it.
enum class LightingDebugView : uint8_t {
  /// @brief The finished image. Every other entry replaces it.
  None,
  /// @brief What the probe volume answers with, before the surface it lights is applied.
  ///
  /// The bounce alone: no albedo, no Fresnel, no occlusion. Those are what the beauty pass does with
  /// this value, and they are also what hides its faults, since a dark wall scales an error down and
  /// a bright one scales it up. Read on its own it is the field itself, and the artefacts that
  /// belong to the field -- leakage through a wall, the lattice printing itself onto a floor, a
  /// pocket of black where nothing could be seen -- appear here at full strength or not at all.
  IndirectDiffuse,
  /// @brief The diffuse irradiance the sky contributes, from the precomputed environment.
  ///
  /// Separate from the bounce because it arrives by a different route and fails in a different way.
  /// This is the first-order term straight off the irradiance cubemap; the bounce is what the probes
  /// gathered, sky included. Seeing them apart is what tells a scene that is too dark outdoors from
  /// one whose skybox never finished loading.
  SkyIrradiance,
  /// @brief Everything reaching the surface straight from a light, shadow applied.
  ///
  /// Not an indirect term, and here for exactly that reason: it is the reference the indirect ones
  /// are judged against. A feature that appears in both belongs to the geometry or the shadow map
  /// rather than to the probe field, which is a question this view settles in one frame and a
  /// beauty-pass argument otherwise settles slowly.
  DirectLight,
  /// @brief The single occlusion factor every indirect term is scaled by.
  ///
  /// Whatever the material's own occlusion map says, which is now the whole of it: the screen space
  /// estimate that used to multiply it has been removed. This is the last value before the ambient
  /// sum, so a crease that reads wrong in the finished image is either wrong here or is not an
  /// occlusion problem at all.
  SurfaceOcclusion,
  /// @brief How much of the probe field the point was allowed to believe, averaged over the cell.
  ///
  /// The Chebyshev test's verdict, white where the corners can all see the point and black where
  /// none of them can. Leakage and its cure both live here: light coming through a wall is this
  /// reading high where it should read low, and a probe lattice printed onto a floor is this varying
  /// abruptly across a cell boundary where it should vary smoothly.
  ProbeVisibility,
  /// @brief What the eight corners summed to before they were normalised.
  ///
  /// The visibility above is a per-corner verdict; this is the whole cell's, after the facing term
  /// and the crush that sends a barely-visible probe the rest of the way to nothing. Where it falls
  /// near zero the reconstruction is deciding the answer from one corner, which is what prints a
  /// cell's shape onto a surface -- visible here as structure before it is visible as shading.
  ProbeWeight,
  /// @brief Red where no corner could see the point and plain interpolation stood in for the test.
  ///
  /// The fallback is a deliberate second-best: returning black would be the very failure the
  /// visibility test exists to prevent, so a point nothing can see is lit by distance alone. It is
  /// correct as a policy and invisible as a symptom, which is a bad combination -- a region shading
  /// entirely through the fallback looks merely wrong rather than broken. This says where it is.
  ProbeFallback,
  /// @brief A colour per octree leaf, darkened by how deep in the tree it sits.
  ///
  /// The subdivision itself, which nothing else shows. Probe spacing halves with every level, so
  /// where the tree refined is where the field can resolve detail and where it did not is where it
  /// cannot. An artefact that follows these boundaries is a placement problem; one that ignores them
  /// is not.
  ProbeCell,
  /// @brief Where in its leaf the point sits, as red, green and blue along the three axes.
  ///
  /// The trilinear coordinate, taken straight. Reads as a linear ramp within each cell, meeting the
  /// next cell's ramp at the face: continuous in value, and with a corner in the gradient there,
  /// which is honest about what trilinear interpolation is. This used to be eased into an S so that
  /// the gradient met itself as well, and the S is what printed the cells onto the shading -- see
  /// `SampleProbeVolume` in scene.hlsl, where the easing was removed and why.
  ProbeBlend
};
/// @brief What the debug spheres standing in for the probes are coloured by.
///
/// The probe field is a buffer, not a target, and no view of the screen shows what is in it. Drawing
/// a sphere per probe and colouring it by one of the probe's own fields is the only way to see the
/// state the trace has built up -- and each of the mechanisms that maintain that state, relocation
/// and dilation among them, is a field of its own that either worked or did not.
///
/// The spheres are drawn into the scene image alongside the geometry rather than into a target of
/// their own, so this is a view rather than a pass and there is nothing in the graph to select.
/// Backends without a probe volume ignore it.
///
/// Ordered to match `EnumTraits<ProbeDebugView>` and the numbering `probe_debug.hlsl` branches on.
enum class ProbeDebugView : uint8_t {
  /// @brief No spheres drawn.
  Off,
  /// @brief The irradiance the probe holds, in the direction each point of the sphere faces.
  ///
  /// The field's actual content, and the one view where a probe reads as what it is: a little
  /// panorama of the light arriving where it stands. A probe that has gone wrong is usually obvious
  /// here first -- black where it should be lit, or carrying the colour of a wall it is buried in.
  Irradiance,
  /// @brief Mean distance to the scene in each direction, scaled against the volume's own reach.
  ///
  /// The other half of what a probe stores, and the half the visibility test runs on. If a wall
  /// leaks light, this is where to look before anything else: the probe either measured the wall or
  /// it did not, and no amount of tuning in the reconstruction can recover a distance that was never
  /// recorded.
  Distance,
  /// @brief Spread of the distances behind each texel, which is what softens the visibility test.
  ///
  /// Near zero where a texel saw one flat surface and large where it straddled an edge, so this maps
  /// out exactly where the test is confident and where it hedges. A silhouette that aliases in the
  /// shading shows up here as a hard line that should have been a soft one.
  Variance,
  /// @brief How much of its own estimate the probe is trusted with, from black to white.
  ///
  /// A probe sealed inside solid geometry sees back faces in every direction and reports the dark
  /// interior of a solid; it is filled in from its neighbours instead. This is the weight that
  /// decides between the two, so the dark spheres here are precisely the probes that are being
  /// spoken for rather than heard. A wall showing a solid sheet of them is a wall too thin for the
  /// spacing, which is a scene problem rather than a renderer one.
  Trust,
  /// @brief How far the probe has walked from its lattice corner, against how far it was allowed to.
  ///
  /// Relocation is what rescues a probe the lattice put inside a wall, and it is silent when it
  /// works. Green is a probe that never had to move, red one that has spent its whole allowance and
  /// is still where it does not want to be -- and a row of red along a wall says the allowance is
  /// too small for the geometry rather than that the mechanism is broken.
  Relocation,
  /// @brief The three states a probe can be in, as three flat colours.
  ///
  /// Green for a probe standing in open space and speaking for itself, blue for one that escaped a
  /// surface and is now fine, red for one still sealed and being filled in from its neighbours. The
  /// summary view: it answers "is the volume healthy" at a glance, and the three views above answer
  /// "why not" once the answer is no.
  Classification
};
} // namespace kuki
