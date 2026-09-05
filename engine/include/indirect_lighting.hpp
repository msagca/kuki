#pragma once
#include <cstdint>
namespace kuki {
/// @brief Every value the indirect lighting is shaped by, as a component on an entity.
///
/// A component rather than a panel of its own, because that is what it is: a thing in the scene with
/// properties, which the hierarchy lists and the properties window edits like any other. The editor
/// puts it on a `Settings` entity so there is somewhere to find it, and `RenderingSystem::Update`
/// hands it to whichever backend is running.
///
/// Not in `SERIALIZED_TYPES`, so it never travels inside a scene file. What a renderer is being
/// asked to do while somebody looks at it is not part of the work, and a scene carrying one
/// person's half-finished tuning onto another machine would override the settings there.
///
/// These were constants until it became clear what the difficulty with them is. Each one is a
/// tradeoff between two artefacts rather than a value with a right answer -- a surface bias too
/// small seals a corner in shadow and one too large detaches the bounce from the geometry, and
/// which of those is worse depends on the scene. A constant states an answer; the answer is only
/// arrived at by moving the value and watching, so the value has to be movable.
///
/// The defaults are the constants that were here before, so a build that never opens the panel
/// renders exactly what it rendered. Each field names the constant it replaced, which is where the
/// reasoning behind the default is written out at length.
///
/// Split by what changing one costs, because that is what the person turning the knob needs to know
/// and nothing else about them differs:
///
/// - The reconstruction group is read per pixel out of `DXFrameConstants`, against a field that has
///   already been traced. Moving one repaints the next frame.
/// - The trace group is read by the compute pass that fills the probes, out of
///   `DXProbeTraceConstants`. Moving one changes what gets stored, so the running mean is wound
///   back to `PROBE_REACTIVE_SAMPLES` and the field takes a few dozen frames to settle again. This
///   is the same treatment a light moving gets, and it happens on its own: the trace hash covers
///   this group.
///
/// The OpenGL backend reads `skyIntensity` and `ambientFallback`, and nothing else. Those two
/// describe light arriving by a route it has -- an irradiance cubemap, and the flat term for a
/// scene with no sky at all -- while everything from `bounceIntensity` down describes a probe
/// field that backend has not got. The editor hides what it cannot reach rather
/// than drawing the slider anyway; see `RendererCapabilities::probeVolume`.
///
/// Not saved to `EngineConfig` either, for the reason the debug views are not: a session that comes
/// back with the bounce turned down to a tenth and no memory of having done it is a renderer that
/// looks broken. Removing the component and letting the editor put a fresh one back is the way to
/// the defaults, and restarting is another.
struct IndirectLighting {
  /// @brief What the probe volume's bounce is multiplied by before it is added to the ambient term.
  ///
  /// The one field here with no constant behind it: the bounce used to be added at unity with no
  /// way to say otherwise. It is first because it is the one to reach for first -- most of the
  /// time the question is whether there is too much indirect light or too little, and that is a
  /// question about this rather than about how the field was gathered.
  float bounceIntensity{1.f};
  /// @brief What the sky's diffuse contribution is multiplied by, on the same footing.
  ///
  /// Separate from the bounce because the two arrive by different routes and a scene can easily
  /// want more of one and less of the other. Having both is also how the balance between them is
  /// found, which no single overall indirect scale would let you do.
  float skyIntensity{1.f};
  /// @brief The flat ambient a scene falls back on with no sky, no ambient light and no volume.
  ///
  /// Replaces the bare `0.03` that both scene shaders had written into them. It shades nothing in
  /// a scene that has any of the three, so it is last in the group and mostly of interest while
  /// setting one of them up. One of the two values every backend reads, being the one case that
  /// needs no probe field and no sky to arrive by.
  float ambientFallback{.03f};
  /// @brief A step off the surface before the field is sampled, as a fraction of the leaf spacing.
  ///
  /// `PROBE_SURFACE_BIAS`. Read both by the shading pass and by the trace's own sampling of the
  /// field for second-bounce light, which is why it winds the mean back despite sitting in the
  /// reconstruction group.
  float probeSurfaceBias{.2f};
  /// @brief Below this a probe's say is crushed towards nothing rather than merely reduced.
  ///
  /// `PROBE_WEIGHT_FLOOR`. The leak guard: raise it to stop light coming through a thin wall, and
  /// watch for the octree lattice starting to print itself onto flat surfaces as it gets high
  /// enough that single corners begin deciding whole cells.
  float probeWeightFloor{.2f};
  /// @brief Exponent the Chebyshev verdict is raised to before it is believed.
  ///
  /// Was a hardcoded cube in `ProbeVisibility`. One is the statistical answer and leaks; higher
  /// values distrust a partly-visible probe more sharply, at the cost of darkening the creases the
  /// test is least certain about. Three is where it was.
  float probeVisibilitySharpness{3.f};
  /// @brief How tightly a ray must line up with a texel's direction to count towards it.
  ///
  /// `PROBE_DEPTH_SHARPNESS`. Tied to the visibility map's width rather than free: the comment on
  /// the constant derives 28 from 256 texels, and sharpening much past it closes the lobe inside
  /// the ray spacing and leaves texels that no ray feeds.
  float probeDepthSharpness{28.f};
  /// @brief How far a probe's visibility reaches, as a fraction of the volume's side.
  ///
  /// `PROBE_DEPTH_RANGE`. The build seeds fresh probes from the constant rather than from this, and
  /// does not need to: the seed carries no weight in the running mean, so the first trace after a
  /// build replaces it outright whatever it was.
  float probeDepthRange{.5f};
  /// @brief How far a ray may travel, as a multiple of the volume's side.
  ///
  /// Was the bare `side * 2` in `DXProbeVolume::Trace`. Shortening it is the cheapest thing here:
  /// a ray that reaches nothing costs the same as one that does.
  float rayDistanceScale{2.f};
  /// @brief How far a hit is nudged along its normal before it is shaded, as a fraction of a cell.
  ///
  /// Was the bare `* .1f` in `DXProbeVolume::Trace`. The trace's equivalent of a shadow bias, and
  /// what stops a hit shading itself out of its own surface.
  float hitNormalNudge{.1f};
  /// @brief Estimates the running mean is worth, so a fresh one is folded in at `1 / (n + this)`.
  ///
  /// `PROBE_MEAN_STEP`. Lower converges faster and holds more of the ray set's noise; higher is
  /// steadier and slower to notice that the light changed. Also scales how far back the mean is
  /// wound when something does change, since `PROBE_REACTIVE_SAMPLES` is a multiple of it.
  uint32_t meanStep{4};
  /// @brief How far a probe steps towards open space each trace, as a fraction of its allowance.
  ///
  /// `PROBE_RELOCATION_MARGIN`. The allowance itself is fixed at build time into each probe's
  /// anchor, so it is not here -- see the note in the panel.
  float relocationMargin{.5f};
  /// @brief How much of each relocation step is held back, which is what keeps a probe from
  /// oscillating between two placements. `PROBE_RELOCATION_DAMPING`.
  float relocationDamping{.25f};
  /// @brief How much the rays coming back off a far side must agree before their mean is taken for
  /// a direction rather than the residue of a ray set. `PROBE_ESCAPE_AGREEMENT`.
  float escapeAgreement{.2f};
  /// @brief What fraction of a probe's rays may come back off the far side of a surface before it
  /// is disbelieved and filled in from its neighbours. `PROBE_BURIED_LIMIT`.
  float buriedLimit{.25f};
  /// @brief How much light must still be getting through before a ray stops being followed.
  ///
  /// `OPAQUE_ENOUGH`. Raising it ends alpha-blended chains sooner, which is the one trace value
  /// here that is purely a cost control.
  float opaqueThreshold{.05f};
};
} // namespace kuki
