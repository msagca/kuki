#pragma once
#include <cstdint>
namespace kuki {
/// @brief Deepest the probe octree may subdivide, which also fixes the lookup grid's resolution.
///
/// Every leaf corner lands on a lattice of `1 << PROBE_OCTREE_MAX_DEPTH` cells per axis, whatever
/// depth the leaf sits at, because a regular octree only ever halves. That is what lets the lookup
/// grid be exactly this fine and still map each of its cells to one leaf and no more.
///
/// Each step costs eight times the nodes in the regions that take it, so this is the knob that
/// decides how much the volume costs. Five is 32 cells per axis and a lattice of at most 35937
/// probe positions, of which a real scene uses a small fraction.
///
/// It is also what makes the tree cheap to keep on the GPU. A bounded depth bounds the node count,
/// so every node the tree could ever have is given a fixed slot up front and a node's index is
/// arithmetic on where it is rather than a pointer something had to allocate. See
/// `PROBE_OCTREE_NODE_COUNT`.
///
/// Here rather than in `dx_probe_volume.hpp` because the update shader computes node indices from
/// it and has to agree with the C++ that sizes the buffers, to the node. It reaches the shader as
/// `KUKI_PROBE_OCTREE_MAX_DEPTH` through `SHADER_DEFINITIONS`.
inline constexpr uint32_t PROBE_OCTREE_MAX_DEPTH = 5;
/// @brief Cells per axis at the deepest level, which is the lookup grid's resolution.
inline constexpr uint32_t PROBE_LATTICE_CELLS = 1u << PROBE_OCTREE_MAX_DEPTH;
/// @brief Probe positions per axis: one more than the cells, since a probe stands on a cell corner.
inline constexpr uint32_t PROBE_LATTICE_CORNERS = PROBE_LATTICE_CELLS + 1;
/// @brief Every probe position the lattice has, which is the most probes a volume can ever need.
inline constexpr uint32_t PROBE_LATTICE_POINTS = PROBE_LATTICE_CORNERS * PROBE_LATTICE_CORNERS * PROBE_LATTICE_CORNERS;
/// @brief Every node a complete octree of `PROBE_OCTREE_MAX_DEPTH` levels has: one, eight, sixty-four...
///
/// The nodes are stored level by level, each level in row-major order of its cells, so the node for
/// cell `c` at depth `d` is at `(8^d - 1) / 7 + c.x + n * (c.y + n * c.z)` with `n = 1 << d`. Its
/// children and its parent are found the same way, and nothing is ever allocated: splitting a leaf
/// flips its state and the states of the eight slots below it.
///
/// The thesis this volume follows allocates instead -- node clusters claimed from a buffer with a
/// compare and swap, so the tree can grow to any depth in whatever memory it is given. At a fixed
/// depth of five that buys nothing. The complete tree is 37449 nodes and 3.6 MB, which is less than
/// a third of what the probes in an ordinary scene take, and in exchange there is no allocator to
/// race, no fragmentation, and no way for a split to fail for want of a node.
inline constexpr uint32_t PROBE_OCTREE_NODE_COUNT = ((1u << (3 * (PROBE_OCTREE_MAX_DEPTH + 1))) - 1) / 7;
/// @brief Lines each thread of the measuring pass casts through its node per frame.
///
/// Sixty-four threads to a group, so this is a hundred and twenty-eight lines a node a frame. See
/// `PROBE_MEASURE_LINES` for how many it takes before a node is judged, and the measuring pass in
/// `probe_update.hlsl` for what a line is.
inline constexpr uint32_t PROBE_MEASURE_LINES_PER_THREAD = 2;
} // namespace kuki
