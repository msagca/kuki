#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <cstddef>
#include <cstdint>
#include <dx_common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <indirect_lighting.hpp>
#include <primitive.hpp>
#include <random>
#include <span>
#include <unordered_map>
#include <vector>
namespace kuki {
class DXAccelerationStructure;
class DXContext;
class DXPipelineCache;
/// @brief Deepest the octree may subdivide, which also fixes the lookup grid's resolution.
///
/// Every leaf corner lands on a lattice of `1 << PROBE_OCTREE_MAX_DEPTH` cells per axis, whatever
/// depth the leaf sits at, because a regular octree only ever halves. That is what lets the lookup
/// grid be exactly this fine and still map each of its cells to one leaf and no more.
///
/// Each step costs eight times the nodes in the regions that take it, so this is the knob that
/// decides how much the volume costs. Five is 32 cells per axis and a lattice of at most 35937
/// probe positions, of which a real scene uses a small fraction.
inline constexpr uint32_t PROBE_OCTREE_MAX_DEPTH = 5;
/// @brief Depth every region subdivides to before triangle density is consulted at all.
///
/// Triangle count alone is the wrong measure below a certain scale, and a Cornell box shows why: six
/// quads is twelve triangles, so no threshold worth applying to a detailed mesh ever splits it, and
/// the volume ends up with eight probes around a room whose lighting varies across every wall.
/// Density is a reason to add probes, not the reason to have them.
///
/// So the volume is a uniform grid at this depth with an adaptive tree on top: three levels give
/// eight cells per axis everywhere, including empty air, and dense geometry keeps subdividing from
/// there. This is the floor on probe spacing, and the octree above it is the part that varies.
inline constexpr uint32_t PROBE_OCTREE_MIN_DEPTH = 3;
/// @brief Triangle count above which a node past the minimum depth keeps subdividing.
///
/// This is the whole of "denser probes in dense geometry": past `PROBE_OCTREE_MIN_DEPTH` a node
/// holding more than this splits and its children get probes at half the spacing, while a node
/// holding fewer stops and keeps the uniform spacing. Nothing else steers the adaptive part.
inline constexpr uint32_t PROBE_OCTREE_LEAF_TRIANGLES = 32;
/// @brief Triangles per cluster, which is how much geometry one box in the subdivision stands for.
///
/// Chosen by measurement rather than by reasoning, because the reasoning points the wrong way.
/// Bigger clusters are looser boxes, so the obvious expectation is that they cost accuracy; on a
/// 949k-triangle scene the tree instead lands closest to the per-triangle one right here, within a
/// quarter of a percent on probe count, and drifts *away* from it in both directions. Smaller
/// clusters under-count, since a box only partly inside a cell contributes only part of its
/// triangles and nothing gives those back; larger ones over-count as the boxes swallow empty space.
///
/// Subdivision cost falls monotonically as this grows, but flattens past here — 128 and 512 differ
/// by under one percent in time while 512 inflates the volume by a tenth. So this is the point
/// where the curve stops paying and the tree is still the one that was wanted.
inline constexpr uint32_t PROBE_CLUSTER_TRIANGLES = 128;
/// @brief Frames the geometry must hold still before a changed scene is rebuilt.
///
/// The same debounce `RenderingSystem::SetResolution` uses, for the same reason: a continuous edit
/// arrives as a burst of distinct values, and only the one it settles on is worth acting on.
inline constexpr uint32_t PROBE_REBUILD_SETTLE_FRAMES = 5;
/// @brief How far the scene may shrink inside the volume before the volume is resized to suit it.
///
/// The volume is kept where it is while the scene fits inside it, so that probes keep their places
/// and their accumulated light across a rebuild -- see `DXProbeVolume::CarryOverProbes`. Holding on
/// forever would be wrong in the other direction: a scene that shrinks to a corner would be sampled
/// by a lattice mostly covering space that is no longer there.
///
/// A half is a wide band on purpose. Resizing is the expensive answer -- it renames every probe --
/// so it is worth reserving for a scene that has genuinely become a different size, rather than one
/// that merely put something down.
inline constexpr float PROBE_VOLUME_KEEP_FRACTION = .5f;
/// @brief Rays each probe casts per frame. Must match `RAYS_PER_PROBE` in `probe_trace.hlsl`.
///
/// Also the thread group size: one group per probe, one thread per ray, so the group can reduce its
/// rays into coefficients through shared memory instead of atomics. Sixty-four rays sample a sphere
/// far too sparsely on their own; the per-frame rotation and the temporal blend are what turn the
/// sequence of sparse estimates into a converged one.
inline constexpr uint32_t PROBE_RAY_COUNT = 64;
/// @brief How fast a fresh estimate is folded in, as `PROBE_MEAN_STEP / (n + PROBE_MEAN_STEP)`.
///
/// The weight has to shrink with the number of estimates `n` already in the probe, or the field
/// never converges. Sixty-four rays sample a sphere far too sparsely to be right on their own, and
/// a blend weight that does not shrink leaves a fixed fraction of that error permanently resident,
/// redrawn every frame by the per-frame ray rotation: shading that keeps shifting under a scene
/// where nothing has moved and no light has changed. A shrinking weight makes the probe the mean of
/// every estimate since the scene last changed, and the error in a mean falls off with `n`.
///
/// One would be the plain running mean, and it is not enough, because a probe's estimate is not
/// independent of the probes: a ray's hit is shaded partly from the field itself, which is what
/// gives the bounces past the first. That loop needs to be iterated to settle, and it settles at a
/// rate set by how much weight the later iterations get. With a plain mean the remaining error
/// falls only as `n` to the power of one minus the scene's albedo — for bright walls, a scene still
/// visibly brightening minutes in. Weighting the step up by this factor raises that power by the
/// same factor, so the bounces are done in a couple of seconds, and costs only the square root of
/// it in noise at a given `n`: four is bounces settled by the time the noise is already quieter
/// than the fixed blend ever was.
inline constexpr uint32_t PROBE_MEAN_STEP = 4;
/// @brief Estimates the count is wound back to when the scene the trace looks at changes.
///
/// Converging is only wanted while the answer is not moving, and a mean allowed to grow without
/// bound would take as long to forget a light as it took to settle on one. So a change winds the
/// count back rather than resetting it, to the count whose weight is the one-in-twenty blend the
/// volume used to run at permanently: a change is chased exactly as quickly as it was before, and
/// the field settles again once the changing stops. Resetting to zero instead would be worse than
/// either, since the next estimate would land whole and unaveraged — the noise of a single trace,
/// as a flash, every frame of a drag.
inline constexpr uint32_t PROBE_REACTIVE_SAMPLES = 19 * PROBE_MEAN_STEP;
/// @brief Byte layout of `Vertex` as `probe_trace.hlsl` hardcodes it to fetch a hit's surface.
///
/// A raw buffer load needs a byte offset and the shader has no reflection to derive one from, so
/// the layout is written into it literally. The assertions below are the only thing tying the two
/// together: change `Vertex` and the build breaks here, rather than the trace quietly interpolating
/// whatever now sits at byte twelve.
inline constexpr uint32_t PROBE_VERTEX_STRIDE = 76;
inline constexpr uint32_t PROBE_VERTEX_NORMAL_OFFSET = 12;
inline constexpr uint32_t PROBE_VERTEX_TEXCOORD_OFFSET = 24;
static_assert(sizeof(Vertex) == PROBE_VERTEX_STRIDE);
static_assert(offsetof(Vertex, normal) == PROBE_VERTEX_NORMAL_OFFSET);
static_assert(offsetof(Vertex, texture) == PROBE_VERTEX_TEXCOORD_OFFSET);
/// @brief Coefficients a probe stores per colour channel. Nine is the second spherical harmonic band.
///
/// Two bands would be four coefficients and could not represent a directional bounce; three bands
/// reconstruct irradiance over a hemisphere to within a few percent, which is the accuracy the
/// technique is usually quoted at. Beyond that the coefficients cost more than the error they remove.
inline constexpr uint32_t PROBE_SH_COEFFICIENTS = 9;
/// @brief Texels across one axis of a probe's visibility map.
///
/// The map is an octahedron unfolded into a square, so the whole sphere costs this squared.
///
/// What decides this is not the ray count. Measured against exact geometry, widening the map from
/// eight to sixteen while sharpening the filter to match took the visibility a probe buried in a
/// wall is granted from 0.53 to 0.05, where nought is the right answer -- and raising the rays from
/// sixty-four to four thousand at either width changed that figure in the third decimal. A texel
/// answers "how far is the scene this way", and a wide texel answers for a cone twenty-five degrees
/// across, which near a corner takes in the floor as well as the wall and reports neither. It is the
/// width of the cone that has to come down, and the rays only have to be dense enough to fill it.
/// Sixteen is where that stops paying: thirty-two needs a filter narrower than sixty-four rays can
/// feed, and measured barely better.
inline constexpr uint32_t PROBE_DEPTH_RESOLUTION = 16;
inline constexpr uint32_t PROBE_DEPTH_TEXELS = PROBE_DEPTH_RESOLUTION * PROBE_DEPTH_RESOLUTION;
/// @brief Words the backface share packs into, a byte to a depth texel.
///
/// A byte because the share is a fraction and the test it feeds is a multiply: an eighth of a
/// percent is far below what a difference in the bounce can be seen at, and a separate array of
/// them costs 256 bytes against the 824 the record has spare.
inline constexpr uint32_t PROBE_BEHIND_WORDS = PROBE_DEPTH_TEXELS / 4;
/// @brief How far a probe's visibility reaches, as a fraction of the volume's side.
///
/// Mirrored by `PROBE_DEPTH_RANGE` in probe_trace.hlsl, which clamps to it. Distances are clamped
/// rather than kept so that a ray escaping the scene cannot drag a probe's mean distance out to the
/// far plane and take the variance with it.
///
/// The clamp needs room above the furthest the test is ever applied, which is one leaf diagonal, or
/// about 1.73 times the coarsest leaf's spacing. Room, rather than merely clearing it: a texel
/// averages a cone some twenty-five degrees wide, so the distances feeding one can be twice what the
/// direction at its centre measures. Sited too close, the grazing rays in that cone clamp while the
/// central one does not, the mean comes out under the true distance, and the probe reports an
/// occluder that is not there -- in a ring, at whatever radius the clamp starts to bite, since a
/// contour of constant distance from a probe meets a flat floor in a circle. This is four times the
/// coarsest spacing against a test that reaches 1.73 of it. The cost of the headroom is a wider
/// spread of distances in each texel and so a more forgiving test, which is the safe way to be
/// wrong.
inline constexpr float PROBE_DEPTH_RANGE = .5f;
/// @brief How far a probe may stray from its lattice corner, as a fraction of its leaf's extent.
///
/// A probe placed on a lattice lands wherever the lattice says, which for the shell around a scene
/// is inside the walls. Such a probe sees the unlit backs of surfaces and reports darkness, and no
/// amount of care in reconstruction rescues it -- the visibility test can only decline to believe
/// it, which is a decision that varies from point to point and prints its own boundary. Letting the
/// probe walk out into the room instead leaves nothing to decline.
///
/// The extent is half a leaf, so half of it is a quarter of the spacing between probes. That is far
/// enough to clear the walls a lattice shell straddles and short enough that a probe still stands
/// for the corner it is interpolated as being at. A probe cornering several leaves takes the
/// smallest allowance any of them grants, which is why it is stored per probe rather than derived.
inline constexpr float PROBE_RELOCATION_LIMIT = .5f;
/// @brief How far a probe steps towards open space each trace, as a fraction of its allowance.
///
/// A step rather than a jump to just past the surface it is escaping. The distance to that surface
/// is known only through whichever ray happened to find it, and the ray set is turned by a fresh
/// rotation every trace, so a target built from it is redrawn every frame and the probe chases it
/// instead of arriving. Walking out at a fixed rate reaches open space in a handful of traces and
/// then stops, because the escape direction goes to nought once nothing faces away.
inline constexpr float PROBE_RELOCATION_MARGIN = .5f;
/// @brief How much the rays that came back off a far side must agree before their mean is a
/// direction to walk in, as the length of their mean direction.
///
/// A probe standing just outside a surface sees the back of it across a clean hemisphere, and the
/// mean of those rays is half a unit long and points away from it. A probe sealed inside solid
/// geometry sees the inside of it in every direction at once; the mean of all sixty-four rays is
/// 0.0016, which is not a direction but the residue of a ray set that does not sum to exactly
/// nothing. The two are three hundred times apart, so where the line falls hardly matters -- what
/// matters is that there is one. Without it the residue is normalised into a full length step, and
/// since the rays are turned by a fresh rotation every trace it points somewhere new each frame,
/// which is a probe that walks at random for as long as the scene is open.
///
/// Placed nearer the quiet end of the gap. A probe that could have escaped and stayed put is one its
/// neighbours cover for through `trust`; a probe that moves when it should not is in the picture.
inline constexpr float PROBE_ESCAPE_AGREEMENT = .2f;
/// @brief How much of a relocation is held back each trace.
///
/// Nought would move a probe straight to where it wants to be, which is right whenever it can get
/// there. A probe sealed inside solid geometry cannot, and would spend every frame proposing a
/// different way out; holding a quarter of the step back settles it on one.
inline constexpr float PROBE_RELOCATION_DAMPING = .25f;
/// @brief What fraction of a probe's rays may come back off the far side of a surface before its own
/// estimate is disbelieved and its neighbours' used instead.
///
/// Relocation rescues probes that can reach open space. One sealed inside solid geometry cannot, and
/// there is nothing to be learned by tracing it: every ray meets a back face and it reports the dark
/// interior of a solid. Rejecting such a probe during reconstruction is what the visibility test was
/// left doing, and rejection is a decision that changes from point to point, which is precisely how
/// it draws its own edge onto a wall. Filling the probe in from its neighbours instead leaves nothing
/// to reject: every probe holds a plausible value, and shading interpolates them all evenly.
///
/// A quarter separates the cases with room to spare. A probe in open air sits near nought, one just
/// inside a surface sees about half its sphere blocked, and nothing real sits close enough to the
/// line for the per-frame rotation of the rays to walk it across.
inline constexpr float PROBE_BURIED_LIMIT = .25f;
/// @brief Neighbours a probe borrows from, one per face of the lattice.
inline constexpr uint32_t PROBE_NEIGHBOURS = 6;
/// @brief Icosphere subdivisions the debug spheres are drawn with.
///
/// One level is eighty triangles, which is round enough for something a few pixels wide and cheap
/// enough that hundreds of them cost less than one character. Each level past this multiplies the
/// count by four for detail no one can see at this size.
inline constexpr uint32_t PROBE_DEBUG_SPHERE_LEVEL = 1;
/// @brief Diameter the debug spheres are drawn at, as a fraction of the finest probe spacing.
///
/// Measured against the spacing a leaf at `PROBE_OCTREE_MAX_DEPTH` would have rather than the
/// spacing this volume actually used, so the spheres never touch however deep the tree went, and
/// a scene that subdivides further does not turn the view into a solid mass.
inline constexpr float PROBE_DEBUG_SPHERE_SCALE = .5f;
/// @brief State the volume's buffers rest in, readable by both the trace and the scene pass.
///
/// The trace is a compute dispatch and shading is a pixel shader, and a buffer read by both has to
/// name both stages: a resource left in the compute-only read state and then sampled while
/// rasterising is a validation error at best and stale data at worst. Only the probe buffer ever
/// leaves this state, for the trace that writes it.
inline constexpr D3D12_RESOURCE_STATES PROBE_READ_STATE = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
/// @brief One octree node as the shaders read it.
///
/// `children` and `probes` are never both meaningful: an interior node has eight children and no
/// probes, a leaf has eight corner probes and no children. They are kept apart rather than shared
/// so a node can be read in a debugger without first deciding which one it is.
///
/// A leaf's probes are listed in the corner order the low three bits of the index give: bit zero is
/// the positive x corner, bit one positive y, bit two positive z. Children use the same order, so a
/// descent and an interpolation index the two arrays the same way.
struct DXOctreeNode {
  float center[3]{};
  float extent{};
  uint32_t children[8]{};
  uint32_t probes[8]{};
  uint32_t depth{};
  uint32_t leaf{};
  uint32_t padding[2]{};
};
/// @brief One probe: where it sits, and the irradiance arriving there as spherical harmonics.
///
/// The coefficients are stored as four floats each rather than three. A structured buffer packs
/// tightly and would let a `float3` straddle nothing at all, so the padding buys no alignment; it
/// buys the shader a `float4` load per coefficient instead of a three-component one, which is the
/// access every reconstruction does nine times in a row.
///
/// `position[3]` is left for the validity weight the trace will write, which is what tells a
/// reconstruction to ignore a probe that ended up inside geometry.
///
/// `depth` is the probe's own view of how far away the scene is, over an octahedral map of
/// directions: the mean distance its rays travelled and the standard deviation of that distance,
/// each a half and the pair packed into one word. Two numbers rather than one because a spread is
/// what turns "the surface is usually about this far away" into a bound on how likely a point
/// further off is to be hidden. It is what stops interpolation blending a probe into a point it
/// cannot see, which is the whole of leakage.
///
/// The deviation is stored rather than the mean square it comes from. A structured buffer element
/// cannot exceed 2048 bytes and two floats a texel would need 2208, but the precision argues for it
/// even where the size does not: recovering a spread of a centimetre by subtracting two metre-scale
/// numbers throws away most of the digits that distinguish them, and at half precision would throw
/// away all of them. Measured against exact geometry the packed pair scores no worse than two full
/// floats, and slightly better than the mean square did.
/// `behind` is the third number the visibility test needs and the two moments cannot hold: for
/// each texel, the share of the rays behind it that met their surface from the far side. Distance
/// is blind to which face of a surface it measured, and for geometry with no thickness -- a wall
/// built from one quad, which is most walls -- that blindness is total. A probe outside such a
/// wall records the distance to it; a point on the inner face stands at that same distance plus
/// only the normal bias, so the mean it is compared against is very nearly its own, the excess
/// Chebyshev is given is a fraction of a probe spacing, and the bound comes back near one. The
/// probe is then believed through a wall, in proportion to a trilinear corner share, which is
/// periodic in the lattice -- and a periodic residual is the grid.
///
/// Orientation settles what distance cannot, and settles it exactly: a ray that met its surface
/// from behind proves the probe is on the far side of that surface, whatever the surface's
/// thickness. The trace has always computed this to decide which probes to relocate; this keeps
/// it. Stored as a share rather than a verdict so that a texel whose cone straddles an edge reads
/// as the mixture it is, and averaged on the same schedule as the moments so it converges with
/// them instead of flickering as the ray set rotates.
///
/// `anchor` is the lattice corner the probe belongs to and is interpolated as being at, which
/// `position` is free to depart from by `anchor[3]`. Keeping both is what lets the probe move: the
/// trilinear weights come from the leaf's own box rather than from where its probes sit, so a probe
/// that steps aside changes what it sees without changing what it is worth to anything.
/// `position[3]` is how much of its own estimate the probe is trusted with: one for a probe with an
/// open view, nought for one sealed in geometry, which takes its neighbours' estimate instead. It is
/// never a weight during shading. Weighting the eight corners of a cell by it is a different idea
/// and a worse one -- the decision then varies from one shading point to the next and prints its own
/// boundary across whatever surface it falls on, which is the artefact this exists to remove.
///
/// `neighbours` is the probe one lattice step away along each face, or `0xFFFFFFFF` where the volume
/// ends. Found at build time, because the octree knows where its corners are and the trace does not.
///
/// `exterior` is one where the build found the probe standing behind the scene's surfaces rather
/// than in front of them, and nought where it did not. It is the thing `trust` could not work out
/// for itself. Relocation and the buried test both read the probe's own rays, so both see a probe
/// sealed inside geometry and neither sees one that is merely somewhere irrelevant: outside a wall,
/// past the edge of every surface, with a clear view of nothing but sky. Such a probe is granted an
/// open view because it has one, and a point on the inner face of the wall beside it then takes its
/// estimate at a trilinear share -- and a share that repeats with the lattice is the grid.
///
/// Decided at build time rather than in the trace for two reasons. It is a property of the geometry
/// and cannot change until the geometry does, so a rebuild is exactly when it is worth asking; and a
/// probe with nothing in sight has no ray that could tell it, which is the whole of why its own rays
/// were not enough.
struct DXProbe {
  float position[4]{};
  float anchor[4]{};
  float irradiance[PROBE_SH_COEFFICIENTS][4]{};
  uint32_t depth[PROBE_DEPTH_TEXELS]{};
  uint32_t behind[PROBE_BEHIND_WORDS]{};
  uint32_t neighbours[PROBE_NEIGHBOURS]{};
  float exterior{};
};
static_assert(sizeof(DXProbe) == 36 + PROBE_SH_COEFFICIENTS * 16 + PROBE_DEPTH_TEXELS * 4 + PROBE_BEHIND_WORDS * 4 + PROBE_NEIGHBOURS * 4);
static_assert(sizeof(DXProbe) == 1484);
static_assert(sizeof(DXProbe) <= 2048, "a structured buffer element cannot be larger than this");
/// @brief One placement of a mesh, as the volume build reads geometry.
///
/// Spans rather than copies: the vertices are read only to derive the mesh's clusters, and only
/// the first time it is seen, so the asset's own storage is enough and must simply outlive the
/// call. The address of that storage is also the key the clusters are cached under.
struct DXProbeGeometry {
  std::span<const Vertex> vertices;
  std::span<const unsigned int> indices;
  glm::mat4 transform{1.f};
};
/// @brief Triangles a mesh emitted in one run, reduced to the box that contains them.
///
/// What the octree sorts, instead of the triangles themselves. Subdivision only ever asked one
/// question of a triangle — which cells does it touch — and a box answers it for a whole run at
/// once. The count travels with the box so density still means triangles rather than boxes.
///
/// Built in the mesh's own space and cached there, so moving the mesh transforms eight corners
/// rather than re-reading its geometry.
struct DXProbeCluster {
  glm::vec3 low{};
  glm::vec3 high{};
  uint32_t triangles{};
};
/// @brief Everything a rebuild produces, before any of it reaches the GPU.
///
/// Split out from `Build` so the expensive half can run, and be checked, without a device. What
/// makes a probe volume good or bad is decided entirely here.
struct DXProbeOctree {
  std::vector<DXOctreeNode> nodes;
  std::vector<DXProbe> probes;
  std::vector<uint32_t> lookup;
  glm::vec3 origin{};
  float side{};
  uint32_t leafCount{};
  uint32_t deepestLeaf{};
  uint32_t triangleCount{};
  uint32_t clusterCount{};
};
/// @brief Where irradiance is sampled from and how a shading point finds the samples near it.
///
/// Three structures, each answering a different question, all resident on the GPU.
///
/// The octree decides *where* probes go. A uniform grid has to be fine enough for the most detailed
/// corner of the scene and then pays that price throughout, most of it on empty air. Subdividing
/// only where triangles are dense spends probes where the lighting actually changes quickly, which
/// is the same place the geometry does.
///
/// The lookup grid decides how a shading point *finds* its leaf. Descending the octree per shaded
/// pixel would be a dependent chain of loads down a pointer structure, which is close to the worst
/// thing a shader can do. The grid is a flat array at the octree's finest resolution holding the
/// index of the leaf covering each cell, so the same question is answered by one address
/// computation and one load, whatever depth the answer happens to live at.
///
/// The probe buffer holds the result. It is written by the trace and read by shading, and is the
/// only one of the three that ever changes after a build.
///
/// The whole volume is rebuilt when the scene's geometry changes and left alone otherwise, so the
/// cost falls at load time rather than per frame. Skinned meshes are excluded for the reason
/// `DXAccelerationStructure` excludes them: their vertices are still in the bind pose here.
class KUKI_ENGINE_API DXProbeVolume final {
public:
  /// @brief Rebuilds the volume over the geometry, or keeps what it has when nothing has changed.
  ///
  /// Sameness is decided by hashing the geometry's identity and transforms, so a camera moving over
  /// a static scene rebuilds nothing and a dragged entity rebuilds everything. Uploading stages
  /// through an upload heap and flushes the command list, so this must run before any pass binds
  /// pipeline state.
  ///
  /// A change does not rebuild straight away. Dragging an entity changes the geometry on every
  /// frame of the drag, and rebuilding each time would pay for a volume that is thrown away one
  /// frame later; the last one is the only one anybody sees. So a new hash starts a settle timer
  /// and the rebuild happens once the geometry has held still for `PROBE_REBUILD_SETTLE_FRAMES`.
  /// The existing volume keeps being sampled in the meantime, which is why the delay is invisible:
  /// bounced light lagging a moving object by a few frames is not something the eye picks up.
  ///
  /// The first build is not delayed. There is nothing to sample until it has happened.
  ///
  /// @return False when the scene has no triangles to place probes around, or an upload failed.
  auto Build(DXContext &, const std::vector<DXProbeGeometry> &) -> bool;
  /// @brief Moves what the resident probes have learned into the ones a rebuild has just produced.
  ///
  /// Matched on the anchor, which is where a probe stands in the volume rather than where it sits
  /// in the array: a rebuild renumbers everything, and subdividing one cell shifts every index
  /// after it, so the index says nothing about identity and the position says everything.
  ///
  /// A probe with no predecessor keeps the zeroed estimate it was built with and converges from
  /// there, which is correct -- it is somewhere no probe stood before. Those are the few, and they
  /// converge under the blend `Trace` sets from the sample count, so how quickly they catch up is
  /// the same question as how reactive the field is after any change.
  ///
  /// @return How many of the new probes were given a predecessor. Zero means a fresh field: either
  /// the first build, or a volume whose lattice moved out from under every probe it had.
  auto CarryOverProbes(DXContext &, std::vector<DXProbe> &) -> uint32_t;
  /// @brief Samples the lookup grid across the volume once and logs whether it agrees with the tree.
  ///
  /// The three structures are built by three separate passes over the same octree, and a mistake in
  /// any of them produces something that still looks like a volume: a grid cell pointing at an
  /// interior node, a leaf whose probe indices belong to its neighbour, a probe sitting a cell away
  /// from the corner it was placed for. None of that is visible until it shows up as light in the
  /// wrong place, so the agreement is checked directly instead.
  ///
  /// Runs at most once per volume, and stalls the pipeline to read its results back.
  auto Validate(DXContext &, DXPipelineCache &) -> void;
  /// @brief Casts a fresh set of rays out of every probe and folds what they see into its coefficients.
  ///
  /// One thread group per probe, one thread per ray. A ray that hits is shaded from the material at
  /// the hit, the scene's punctual lights with an occlusion ray each, and the volume's own estimate
  /// of the irradiance already arriving there. That last term is what makes the bounce count
  /// unbounded: every trace feeds on the previous one, so light that has bounced twice by one frame
  /// has bounced three times by the next, with no per-bounce cost.
  ///
  /// The dispatch leaves the probe buffer readable again rather than only barriered, because the
  /// scene pass that reads it this frame rasterises: a plain unordered access barrier orders the
  /// writes but leaves the buffer in a state a pixel shader may not read it from.
  ///
  /// The probe buffer is read and written by the same dispatch, so a probe may sample a neighbour
  /// that this frame has already updated. It is a feedback loop either way and the blend absorbs
  /// the difference; a second buffer to ping-pong between would double the memory to remove an
  /// error the hysteresis hides.
  ///
  /// Each estimate is folded in as a running mean rather than at a fixed rate, so the field
  /// converges on a still scene instead of shimmering around the answer forever. What decides when
  /// that mean starts over is the caller's hash: see `PROBE_REACTIVE_SAMPLES`.
  ///
  /// Records into the frame's command list and never stalls, unlike the build. Runs every frame.
  ///
  /// @param frameConstants Address of the lights the hit shading reads, in the scene pass's layout.
  /// @param sceneHash Everything the trace looks at that the camera does not move: the lights, and
  /// the placements and materials in the acceleration structure. A probe's mean is only worth
  /// keeping while the thing it is a mean of holds still, and this is what says whether it has. It
  /// must not fold in the camera, or the field would restart on every mouse movement.
  /// @param settings The values shaping what the trace gathers. Hashed in alongside `sceneHash`, so
  /// changing one winds the mean back exactly as a light moving does -- which is the honest
  /// treatment, since a mean of estimates taken under two different settings is a mean of nothing.
  auto Trace(DXContext &, DXPipelineCache &, const DXAccelerationStructure &, const D3D12_GPU_VIRTUAL_ADDRESS, const D3D12_GPU_VIRTUAL_ADDRESS, const size_t, const IndirectLighting &) -> void;
  /// @brief Reads the probes back once the field has converged and logs what they hold.
  ///
  /// Nothing renders from the probes yet, so a trace that produced black, uniform grey, or the same
  /// colour everywhere would look exactly like one that worked. Reporting the field's mean colour
  /// and how it differs across the volume is what separates those cases before anything depends on
  /// them being right.
  ///
  /// Runs once, after enough frames for the blend to settle, and stalls to read the buffer back.
  auto Report(DXContext &) -> void;
  /// @brief Address of the octree nodes, to bind as a root shader resource view.
  auto GetNodeAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Address of the probes, whose irradiance the trace writes and shading reads.
  auto GetProbeAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Address of the leaf index per lookup cell, which is how shading finds its probes.
  auto GetLookupAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief The volume's minimum corner and the length of its side, which is cubic.
  auto GetBounds(float *, float *) const -> void;
  auto GetProbeCount() const -> uint32_t;
  auto GetNodeCount() const -> uint32_t;
  /// @brief Cells per axis in the lookup grid, which is `1 << PROBE_OCTREE_MAX_DEPTH`.
  auto GetLookupResolution() const -> uint32_t;
  /// @brief Whether the last build left a volume that can be sampled.
  auto IsReady() const -> bool;
  /// @brief Releases every buffer and forgets the geometry it was built from.
  auto Clear() -> void;
  /// @brief Sorts the geometry into an octree and places probes, without touching the device.
  ///
  /// The whole of a rebuild except the upload. Public because it is the part worth measuring: it
  /// decides the node count, the probe count and how the two follow the geometry, none of which a
  /// device is needed to judge.
  auto BuildOctree(const std::vector<DXProbeGeometry> &) -> DXProbeOctree;
private:
  DXContext *owner{};
  ComPtr<ID3D12Resource> nodeBuffer;
  ComPtr<ID3D12Resource> probeBuffer;
  ComPtr<ID3D12Resource> lookupBuffer;
  ComPtr<ID3D12Resource> auditSeed;
  ComPtr<ID3D12Resource> auditResult;
  ComPtr<ID3D12Resource> auditReadback;
  ComPtr<ID3D12Resource> probeReadback;
  /// @brief Staging buffers the command list still has to consume, released once it has been flushed.
  std::vector<ComPtr<ID3D12Resource>> staging;
  uint8_t *auditSeedData{};
  /// @brief Source of the per-frame ray rotation. Seeded fixed, so a run reproduces the one before it.
  std::mt19937 random{1u};
  D3D12_RESOURCE_STATES probeState{PROBE_READ_STATE};
  glm::vec3 origin{};
  float side{};
  size_t geometryHash{};
  /// @brief Hash the geometry has most recently changed to, which may not have settled yet.
  size_t pendingHash{};
  /// @brief Frames `pendingHash` has been unchanged for.
  uint32_t pendingFrames{};
  /// @brief One mesh's clusters in its own space, kept so a moved mesh is not re-read.
  struct MeshClusters {
    std::vector<DXProbeCluster> clusters;
    size_t vertexCount{};
    size_t indexCount{};
  };
  /// @brief Cached clusters, keyed on the address of the mesh's vertices.
  ///
  /// The same identity the geometry hash uses, and it carries the same caveat: a freed mesh whose
  /// storage is reused by another of exactly the same size would be mistaken for the original. The
  /// sizes are held alongside so the common case of a differently shaped mesh is caught.
  std::unordered_map<const void *, MeshClusters> meshClusters;
  uint32_t nodeCount{};
  uint32_t probeCount{};
  uint32_t leafCount{};
  uint32_t deepestLeaf{};
  uint32_t loggedProbeCount{};
  uint32_t tracedFrames{};
  /// @brief Estimates folded into the probes since the last change, which sets the blend weight.
  ///
  /// Not `tracedFrames`: that one only ever counts up, because what it reports is how long the
  /// field has had to settle. This one is wound back whenever the scene changes, and it is the
  /// wind-back that keeps a converged field able to react at all.
  uint32_t tracedSamples{};
  /// @brief The scene hash the accumulated estimates belong to. A different one winds them back.
  size_t traceHash{};
  bool ready{};
  bool validated{};
  bool reported{};
  /// @brief Copies one build's worth of bytes into a default-heap buffer through a staging upload.
  ///
  /// Read every frame by shading and written once here, which is the case a default heap exists
  /// for; an upload heap would put the reads across the bus for the life of the scene.
  ///
  /// @param flags `ALLOW_UNORDERED_ACCESS` for the probe buffer, which the trace will write.
  auto UploadBuffer(const void *, const uint64_t, const D3D12_RESOURCE_FLAGS, const char *) -> ComPtr<ID3D12Resource>;
  /// @brief Creates the seed, result and readback buffers the audit reports through.
  auto EnsureAuditBuffers() -> bool;
};
} // namespace kuki
#endif
