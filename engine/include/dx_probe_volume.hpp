#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <dx_common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <kuki_engine_export.h>
#include <indirect_lighting.hpp>
#include <primitive.hpp>
#include <probe_limits.hpp>
#include <random>
#include <span>
#include <unordered_map>
#include <vector>
namespace kuki {
class DXAccelerationStructure;
class DXContext;
class DXPipelineCache;
/// @brief Probe spacing the uniform part of the volume aims for, in world units.
///
/// The floor on spacing is a distance, not a depth, and the difference is the whole point. A fixed
/// depth is a fixed number of cells per axis, so it means completely different spacing in different
/// scenes: three levels gave a four unit room half a unit between probes and a four hundred unit
/// level fifty. Neither of those was asked for; both were what one constant happened to imply.
///
/// Stated as a distance, the same number means the same thing in both -- a probe roughly every unit
/// -- and the depth is whatever gets there. See `ProbeUniformDepth`.
///
/// One unit is chosen for the scenes here, which are rooms measured in units that read as metres,
/// and a metre is where diffuse indirect light stops changing much between one probe and the next.
/// A scene built at a different scale wants this changed with it; it is the one number in this file
/// that is about the world rather than about the algorithm.
inline constexpr float PROBE_TARGET_SPACING = 1.f;
/// @brief Shallowest uniform grid the volume will settle for, whatever its size implies.
///
/// Two levels is four cells per axis and a hundred and twenty-five probe positions. A volume smaller
/// than the target spacing would otherwise be handed a single cell with eight probes on its corners,
/// and eight probes around a room is not a light field -- it is a box with a colour at each corner.
inline constexpr uint32_t PROBE_OCTREE_MIN_DEPTH_FLOOR = 2;
/// @brief Surface area a node may hold, as a multiple of its own cross-section, before it splits.
///
/// The adaptive half of the tree, and what it measures is deliberately not triangles. Triangle count
/// is a modelling decision: the same wall is two triangles or two thousand depending on who built
/// it, and the irradiance around it is identical either way. Steering on it meant a high resolution
/// model imported at a small scale dragged every cell it touched down to the maximum depth while the
/// low poly room around it stayed coarse -- probes packed most densely exactly where they were least
/// needed, which is the opposite of what the octree was added to do.
///
/// Area answers the question that was meant. A cell is compared against its own face, so the measure
/// is free of both scale and tessellation: one flat surface crossing a cell reads as 1 whatever the
/// cell's size and whatever the surface is made of, two walls meeting in it read as 2, and the
/// corner where three meet reads as 3. Past that a cell is holding genuinely folded or cluttered
/// geometry, where the light field really does vary faster than the uniform spacing can follow.
///
/// Four is therefore just above the busiest arrangement that is still simple. It is a ratio, so it
/// needs no retuning when a scene is built at a different size.
///
/// The area used to be summed on the CPU from the triangles themselves, grouped into boxes of a
/// hundred and twenty-eight. It is now measured on the GPU by casting random lines through the cell
/// and counting the surfaces each one crosses, which by Crofton's formula estimates the same area
/// without reading a vertex -- see `CSMeasure` in `probe_update.hlsl`. The threshold carried over
/// unchanged, which is the point of measuring the same thing.
inline constexpr float PROBE_OCTREE_LEAF_SURFACE = 4.f;
/// @brief How far a measurement must pass the threshold, as a fraction of it, before it is acted on.
///
/// A split needs more than `PROBE_OCTREE_LEAF_SURFACE` by this much and a merge needs less by this
/// much, so a cell measuring close to the line does not split on one estimate and merge on the next.
/// The estimate is a mean over `PROBE_MEASURE_LINES` random lines and its spread on a cell holding a
/// few walls is about a twentieth of the threshold, so a quarter keeps the two decisions five spreads
/// apart -- far enough that re-measuring a cell after a nearby change does not undo what it decided
/// before, which would be a probe appearing and vanishing in time with the clock.
inline constexpr float PROBE_SURFACE_HYSTERESIS = .25f;
/// @brief Lines a node must be measured with before it is split or merged.
///
/// Five hundred and twelve is four frames at a hundred and twenty-eight lines a frame, and about two
/// in three lines survive the test for meeting the cube, so a node is judged after six or seven frames.
/// Fewer and the spread of the estimate starts to reach the hysteresis band; more and a region that a
/// piece has just been put down in takes visibly longer to gain its probes.
inline constexpr uint32_t PROBE_MEASURE_LINES = 512;
/// @brief Most nodes measured in one frame.
///
/// The measuring pass costs a group of sixty-four threads per node and the lines are short, so this
/// is a ceiling on a burst rather than a figure the frame normally pays: once a scene has settled
/// nothing is queued at all, and a change queues only the nodes it reached. What it bounds is the
/// load right after a scene opens, when every node in the uniform grid wants measuring at once; at a
/// thousand a frame a sixteen-unit room's four thousand cells are measured in under a second, with
/// the uniform grid serving as the volume in the meantime.
inline constexpr uint32_t PROBE_MEASURE_BUDGET = 1024;
/// @brief Most boxes of changed geometry uploaded in one frame.
///
/// Past this the boxes are merged into the one box around all of them, which only ever asks for more
/// measuring than was needed, never less. A frame that moves sixty-four separate things is a frame
/// where re-measuring a large region is not going to be the thing anyone notices.
inline constexpr uint32_t PROBE_MAX_DIRTY_BOXES = 64;
/// @brief Probe slots a volume starts with at least, before the uniform grid's own count is consulted.
inline constexpr uint32_t PROBE_POOL_MINIMUM = 4096;
/// @brief Depth every region subdivides to before the surface measure is consulted at all.
///
/// The volume is a uniform grid at this depth with an adaptive tree on top: the grid puts probes in
/// empty air as well as against geometry, because a room's light varies across the middle of it and
/// not only where something is standing, and the tree adds more where the geometry earns them.
///
/// Rounded rather than floored or ceiled: the target is a spacing to land near, so the nearest power
/// of two to it is the honest reading. A four unit room comes out at two levels and a spacing of
/// 1.02; sixteen units comes out at four levels and exactly 1.
///
/// Clamped at both ends. The floor keeps a volume smaller than the target from collapsing to one
/// cell, and the ceiling is the lattice the lookup grid is built on -- a scene far larger than the
/// target simply cannot have the spacing it asked for, and gets the finest the tree can express.
inline auto ProbeUniformDepth(const float side) -> uint32_t {
  if (!(side > .0f))
    return PROBE_OCTREE_MIN_DEPTH_FLOOR;
  const auto levels = std::lround(std::log2(static_cast<double>(side) / PROBE_TARGET_SPACING));
  const auto clamped = std::clamp<long>(levels, PROBE_OCTREE_MIN_DEPTH_FLOOR, PROBE_OCTREE_MAX_DEPTH);
  return static_cast<uint32_t>(clamped);
}
/// @brief How far the scene may shrink inside the volume before the volume is resized to suit it.
///
/// The volume is kept where it is while the scene fits inside it, so that the lattice stays on the
/// same world positions and every probe keeps its place and its accumulated light while the tree
/// changes around it. Holding on forever would be wrong in the other direction: a scene that shrinks
/// to a corner would be sampled by a lattice mostly covering space that is no longer there.
///
/// A half is a wide band on purpose. Resizing is the expensive answer -- it moves the lattice, so no
/// probe stands where it stood and the whole field starts again from the uniform grid -- so it is
/// worth reserving for a scene that has genuinely become a different size, rather than one that
/// merely put something down.
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
///
/// The buffer holds every node the complete tree could have, at the slot `PROBE_OCTREE_NODE_COUNT`
/// describes, so `children` is fixed from the moment the volume is laid down and `leaf` is what says
/// which of those slots are part of the tree right now. Only the GPU ever writes these.
struct DXOctreeNode {
  float center[3]{};
  float extent{};
  uint32_t children[8]{};
  uint32_t probes[8]{};
  uint32_t depth{};
  /// @brief Nought for a slot below a leaf and not in the tree, one for a leaf, two for an interior node.
  ///
  /// Shading only ever tests for one: the lookup grid only points at leaves, so the other two are
  /// for the passes that change the tree.
  uint32_t leaf{};
  /// @brief Mean surfaces crossed per line cast through the node, since it was last disturbed.
  ///
  /// Three times this is the area the node holds against its own face, which is what
  /// `PROBE_OCTREE_LEAF_SURFACE` is compared with.
  float surface{};
  /// @brief Lines that mean is over. Reset to nought by a change nearby, which re-queues the node.
  uint32_t samples{};
};
static_assert(sizeof(DXOctreeNode) == 96, "probe_update.hlsl, probe_trace.hlsl, probe.hlsl and scene.hlsl declare this layout");
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
/// ends. Found by the update rather than the trace, because the lattice knows where its corners are
/// and the trace does not.
///
/// `exterior` is how far the probe is standing behind the scene's surfaces rather than in front of
/// them, from nought to one. It is the thing `trust` could not work out for itself. Relocation and the
/// buried test both read the probe's own rays near it, so both see a probe sealed inside geometry and
/// neither sees one that is merely somewhere irrelevant: outside a wall, past the edge of every
/// surface, with a clear view of nothing but sky. Such a probe is granted an open view because it has
/// one, and a point on the inner face of the wall beside it then takes its estimate at a trilinear
/// share -- and a share that repeats with the lattice is the grid.
///
/// This was decided by the CPU build, by flood-filling the volume from the orientation of every
/// triangle in it, on the reasoning that a probe with nothing in sight has no ray that could tell it.
/// That holds for rays the length of the buried test and not for rays the length of the trace's, which
/// reach twice across the volume: a probe outside a wall sees the back of that wall, however far off
/// it is. So the trace now decides it, from which face of the nearest surface each probe's rays met,
/// on the same schedule as everything else it learns. See the trace's own note on it.
///
/// An allowance of nought in `anchor[3]` marks a slot in the pool that no lattice point holds. The
/// trace and the debug view skip those; nothing else ever reaches one, since no leaf names it.
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
/// @brief One placement of a mesh, as the volume reads geometry.
///
/// The volume no longer reads triangles at all -- the tree is measured on the GPU against the
/// acceleration structure the trace already has -- so what it needs from a placement is only where
/// it is and whether it moved. The vertices are a span into the asset's own storage, read once per
/// mesh for its bounding box and cached under their address.
///
/// `entity` is what a placement is recognised by from one frame to the next. Not the mesh: a clock
/// digit that swaps its mesh is the same placement showing something else, and what has to be
/// re-measured is the space it stood in before and stands in now -- both of which are known only if
/// the placement can be found again under a key the swap did not change.
struct DXProbeGeometry {
  uint64_t entity{};
  std::span<const Vertex> vertices;
  glm::mat4 transform{1.f};
};
/// @brief Where irradiance is sampled from and how a shading point finds the samples near it.
///
/// Three structures, each answering a different question, all resident on the GPU.
///
/// The octree decides *where* probes go. A uniform grid has to be fine enough for the most detailed
/// corner of the scene and then pays that price throughout, most of it on empty air. Subdividing
/// only where the geometry is dense spends probes where the lighting actually changes quickly, which
/// is the same place the geometry does.
///
/// The lookup grid decides how a shading point *finds* its leaf. Descending the octree per shaded
/// pixel would be a dependent chain of loads down a pointer structure, which is close to the worst
/// thing a shader can do. The grid is a flat array at the octree's finest resolution holding the
/// index of the leaf covering each cell, so the same question is answered by one address
/// computation and one load, whatever depth the answer happens to live at.
///
/// The probe buffer holds the result. It is written by the trace and read by shading.
///
/// All three are kept up to date on the GPU, following Fjellstedt and Antoniev's probe placement
/// thesis: rays measure each leaf, leaves split and merge in place, and a change in the geometry
/// re-measures only the nodes it reached. The volume used to be rebuilt on the CPU whenever any
/// placement changed, which cost 550 ms on the chess scene and was paid once a second for as long as
/// its clock ran. See `probe_update.hlsl` for the passes and for where they depart from the thesis.
///
/// What remains on the CPU is deciding where the volume is, noticing which placements moved, and
/// sizing the probe pool. None of it reads anything back synchronously: the counters the GPU keeps
/// come back through a ring of readback buffers a couple of frames late, which is soon enough for
/// everything they are used for.
///
/// Skinned meshes are excluded for the reason `DXAccelerationStructure` excludes them: their
/// vertices are still in the bind pose here.
class KUKI_ENGINE_API DXProbeVolume final {
public:
  /// @brief Notices what moved, records this frame's passes over the tree, and lays the volume down
  /// first if it has none yet or the scene no longer fits the one it has.
  ///
  /// Records into the frame's command list and never stalls. Must run after the acceleration
  /// structure is built for the frame, since the measuring pass traces against it, and before the
  /// trace, which reads the tree this leaves behind. A volume laid down this frame is usable this
  /// frame: it starts as the uniform grid, and the adaptive part grows in over the frames after.
  ///
  /// @return False when there is no volume to sample: no geometry has ever been seen, or the device
  /// could not create the buffers or the passes.
  auto Update(DXContext &, DXPipelineCache &, const DXAccelerationStructure &, const std::vector<DXProbeGeometry> &) -> bool;
  /// @brief Samples the lookup grid across the volume once and logs whether it agrees with the tree.
  ///
  /// The three structures are written by separate passes over the same octree, and a mistake in any
  /// of them produces something that still looks like a volume: a grid cell pointing at an interior
  /// node, a leaf whose probe indices belong to its neighbour, a probe sitting a cell away from the
  /// corner it was placed for. None of that is visible until it shows up as light in the wrong place,
  /// so the agreement is checked directly instead.
  ///
  /// Waits until the tree has first settled after being laid down, since a tree still growing is
  /// expected to be part way through a change. Runs once per volume after that, and stalls the
  /// pipeline to read its results back.
  auto Validate(DXContext &, DXPipelineCache &) -> void;
  /// @brief Casts a fresh set of rays out of every probe and folds what they see into its coefficients.
  ///
  /// One thread group per probe slot, one thread per ray; a slot holding no probe returns at once. A
  /// ray that hits is shaded from the material at the hit, the scene's punctual lights with an
  /// occlusion ray each, and the volume's own estimate of the irradiance already arriving there. That
  /// last term is what makes the bounce count unbounded: every trace feeds on the previous one, so
  /// light that has bounced twice by one frame has bounced three times by the next, with no
  /// per-bounce cost.
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
  /// Records into the frame's command list and never stalls. Runs every frame.
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
  /// @brief Slots in the probe pool, which is what a probe index is bounds-checked against.
  ///
  /// Not how many probes are in use: that is decided on the GPU, and a sampler only needs to know
  /// that an index it reads is inside the buffer. A slot holding no probe is never named by a leaf.
  auto GetProbeCount() const -> uint32_t;
  /// @brief Node slots, which is every node the complete tree has. See `PROBE_OCTREE_NODE_COUNT`.
  auto GetNodeCount() const -> uint32_t;
  /// @brief Cells per axis in the lookup grid, which is `1 << PROBE_OCTREE_MAX_DEPTH`.
  auto GetLookupResolution() const -> uint32_t;
  /// @brief Whether there is a volume that can be sampled.
  auto IsReady() const -> bool;
  /// @brief Releases every buffer and forgets the geometry it was built from.
  auto Clear() -> void;
private:
  /// @brief Where one placement stood the last time it was seen, so a change can be told apart.
  struct Placement {
    const void *mesh{};
    glm::mat4 transform{1.f};
    glm::vec3 low{};
    glm::vec3 high{};
    uint64_t seen{};
  };
  /// @brief A mesh's bounding box in its own space, with the vertex count it was taken from.
  ///
  /// Keyed on the address of the vertices, which carries the usual caveat: a freed mesh whose storage
  /// is reused by another of exactly the same size would be mistaken for the original. The count is
  /// held alongside so the common case of a differently shaped mesh is caught.
  struct MeshBounds {
    glm::vec3 low{};
    glm::vec3 high{};
    size_t vertexCount{};
  };
  DXContext *owner{};
  ComPtr<ID3D12Resource> nodeBuffer;
  ComPtr<ID3D12Resource> probeBuffer;
  ComPtr<ID3D12Resource> lookupBuffer;
  ComPtr<ID3D12Resource> cornerBuffer;
  ComPtr<ID3D12Resource> freeBuffer;
  ComPtr<ID3D12Resource> counterBuffer;
  ComPtr<ID3D12Resource> queueBuffer;
  /// @brief The changed boxes, one region per frame in flight, written through a persistent mapping.
  ComPtr<ID3D12Resource> boxBuffer;
  /// @brief The counters as they stood at the end of each frame in flight, one region per frame.
  ComPtr<ID3D12Resource> counterReadback;
  ComPtr<ID3D12Resource> auditSeed;
  ComPtr<ID3D12Resource> auditResult;
  ComPtr<ID3D12Resource> auditReadback;
  ComPtr<ID3D12Resource> probeReadback;
  uint8_t *boxData{};
  uint8_t *auditSeedData{};
  /// @brief Which generation of the volume each readback region was written under, or nought if none.
  ///
  /// A region is read when its frame slot comes round again, which is after the GPU has finished
  /// with it. Counters written before a reset or a growth describe a pool that no longer exists, so
  /// they are told apart by this and ignored.
  std::array<uint64_t, DX_FRAME_COUNT> readbackGeneration{};
  uint64_t generation{};
  /// @brief Source of the per-frame ray rotation. Seeded fixed, so a run reproduces the one before it.
  std::mt19937 random{1u};
  D3D12_RESOURCE_STATES probeState{D3D12_RESOURCE_STATE_COMMON};
  D3D12_RESOURCE_STATES nodeState{D3D12_RESOURCE_STATE_COMMON};
  D3D12_RESOURCE_STATES lookupState{D3D12_RESOURCE_STATE_COMMON};
  glm::vec3 origin{};
  float side{};
  uint32_t uniformDepth{};
  /// @brief Slots in the probe pool, which grows when the GPU reports running out.
  uint32_t capacity{};
  /// @brief Slots the pool had before a growth recorded this frame, or nought when it did not grow.
  uint32_t grownFrom{};
  uint32_t frameSeed{};
  /// @brief Whether the passes recorded this frame start by laying the tree down from scratch.
  bool resetPending{};
  std::unordered_map<uint64_t, Placement> placements;
  std::unordered_map<const void *, MeshBounds> meshBounds;
  uint64_t geometryFrame{};
  /// @brief Boxes of space whose geometry changed this frame, as minimum and maximum corner pairs.
  std::vector<glm::vec4> dirtyBoxes;
  uint32_t liveProbes{};
  uint32_t leafCount{};
  uint32_t deepestLeaf{};
  /// @brief Whether the last counters read back showed nothing queued, split or merged.
  bool settled{};
  uint32_t loggedProbes{};
  uint32_t loggedLeaves{};
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
  /// @brief Creates a default-heap buffer the update passes write, already in the unordered access state.
  auto CreateBuffer(const uint64_t, const char *) -> ComPtr<ID3D12Resource>;
  /// @brief Creates every buffer whose size does not depend on the pool, if it does not exist yet.
  auto EnsureFixedBuffers() -> bool;
  /// @brief Replaces the probe pool and its free list with ones of the given size, empty.
  auto CreatePool(const uint32_t) -> bool;
  /// @brief Replaces the probe pool and its free list with larger ones, keeping what the old held.
  auto GrowPool(const uint32_t) -> bool;
  /// @brief Notes which placements changed since the last frame and returns the box around all of them.
  auto TrackGeometry(const std::vector<DXProbeGeometry> &, glm::vec3 &, glm::vec3 &) -> bool;
  /// @brief Reads the counters of the frame that last used this slot, and grows the pool if they ask.
  auto ReadCounters(DXContext &) -> void;
  /// @brief Records every update pass, in order, with a barrier between each two.
  auto RecordPasses(DXContext &, DXPipelineCache &, const DXAccelerationStructure &) -> bool;
  /// @brief Moves one of the tracked buffers to a state, if it is not already in it.
  auto Transition(ID3D12Resource *, D3D12_RESOURCE_STATES &, const D3D12_RESOURCE_STATES) -> void;
  /// @brief Creates the seed, result and readback buffers the audit reports through.
  auto EnsureAuditBuffers() -> bool;
};
} // namespace kuki
#endif
