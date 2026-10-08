// Keeps the probe octree on the GPU, one small pass at a time, instead of rebuilding it on the CPU.
//
// The structure follows Fjellstedt and Antoniev, "Probe Placement for Dynamic Diffuse Global
// Illumination" (2024): the tree lives in a GPU buffer, rays cast through each leaf decide whether it
// is holding enough of the scene to split, and the only nodes ever revisited are the ones a change in
// the geometry touched. What it replaced was a rebuild of the whole volume on the CPU whenever any
// mesh moved -- 550 ms on the chess scene, every time the clock's digits changed.
//
// Three departures from the thesis, each for a reason specific to this volume:
//
// - The nodes are not allocated. The depth is capped at five, so every node the tree could have is
//   given a fixed slot and found by arithmetic; see `PROBE_OCTREE_NODE_COUNT` in probe_limits.hpp.
// - Probes stand on leaf corners rather than one in the middle of each leaf, because shading
//   interpolates the eight corners of the leaf a point is in. A corner is shared by every leaf
//   around it, so probes are kept per lattice point and allocated from a free list as leaves come
//   to need them -- which is the one allocation left, and it runs in passes of its own.
// - What a leaf measures is how much surface it holds rather than what share of its rays hit
//   something. See `CSMeasure`.
//
// Each pass reads what the pass before it wrote, and the C++ puts a barrier between every two of
// them. None of them stalls, and none of them reads anything back.
static const uint INVALID_INDEX = 0xFFFFFFFF;
static const uint MAX_DEPTH = KUKI_PROBE_OCTREE_MAX_DEPTH;
static const uint CELLS = 1u << MAX_DEPTH;
static const uint CORNERS = CELLS + 1;
static const uint POINTS = CORNERS * CORNERS * CORNERS;
static const uint NODE_COUNT = ((1u << (3 * (MAX_DEPTH + 1))) - 1) / 7;
static const uint GROUP_SIZE = 64;
static const uint LINES_PER_THREAD = KUKI_PROBE_MEASURE_LINES_PER_THREAD;
static const float PI = 3.14159265359;
// What `leaf` holds. Absent is a slot below a leaf: part of the complete tree the storage is laid out
// as, and not part of the tree the volume actually has.
static const uint STATE_ABSENT = 0;
static const uint STATE_LEAF = 1;
static const uint STATE_INTERIOR = 2;
// What a freshly allocated probe carries in `position.w` until the seed pass has looked at it. Trust
// is never negative, so anything below nought is a probe still being set up; see `CSSeed`.
static const float FRESH = -4.0;
// Slots in the counter buffer. Mirrors `ProbeCounter` in dx_probe_volume.cpp.
static const uint COUNTER_FREE_TOP = 0;
static const uint COUNTER_FAILED = 1;
static const uint COUNTER_PROBES = 2;
static const uint COUNTER_LEAVES = 3;
static const uint COUNTER_DEEPEST = 4;
static const uint COUNTER_QUEUED = 5;
static const uint COUNTER_SPLITS = 6;
static const uint COUNTER_MERGES = 7;
struct OctreeNode {
  float3 center;
  float extent;
  uint4 children[2];
  uint4 probes[2];
  uint depth;
  uint leaf;
  float surface;
  uint samples;
};
struct Probe {
  float4 position;
  float4 anchor;
  float4 irradiance[9];
  uint depth[256];
  uint behind[64];
  uint neighbours[6];
  float exterior;
};
cbuffer UpdateConstants : register(b0) {
  // The volume's minimum corner, and its cubic side in the fourth component.
  float4 u_volume;
  // Probe slots, the uniform depth, how many changed boxes this frame brought, and a per-frame seed.
  uint4 u_counts;
  // Probe slots before a growth, the most nodes measured a frame, the lines a node is judged on, and
  // how far a neighbour search may step along the lattice.
  uint4 u_limits;
  // The surface a node may hold before it splits, the hysteresis either side of it, how far a probe
  // may stray from its corner as a fraction of its leaf's extent, and the distance a fresh probe's
  // visibility is seeded at.
  float4 u_tuning;
};
RaytracingAccelerationStructure g_scene : register(t0);
// Pairs of minimum and maximum corners, one pair for each box of space whose geometry changed.
StructuredBuffer<float4> g_boxes : register(t1);
RWStructuredBuffer<OctreeNode> g_nodes : register(u0);
RWStructuredBuffer<Probe> g_probes : register(u1);
RWStructuredBuffer<uint> g_lookup : register(u2);
// The probe slot standing at each lattice point, or `INVALID_INDEX` where none is.
RWStructuredBuffer<uint> g_corners : register(u3);
// Probe slots no lattice point holds, as a stack whose height is `COUNTER_FREE_TOP`.
RWStructuredBuffer<uint> g_free : register(u4);
RWStructuredBuffer<int> g_counters : register(u5);
// The nodes being measured this frame, as many as `COUNTER_QUEUED` says up to `u_limits.y`.
RWStructuredBuffer<uint> g_queue : register(u6);
groupshared uint gs_lines[GROUP_SIZE];
groupshared uint gs_crossings[GROUP_SIZE];
uint LevelOffset(uint depth) {
  return ((1u << (3 * depth)) - 1) / 7;
}
uint NodeIndex(uint depth, uint3 cell) {
  uint width = 1u << depth;
  return LevelOffset(depth) + cell.x + width * (cell.y + width * cell.z);
}
void DecodeNode(uint index, out uint depth, out uint3 cell) {
  depth = 0;
  while (depth < MAX_DEPTH && index >= LevelOffset(depth + 1))
    ++depth;
  uint local = index - LevelOffset(depth);
  uint width = 1u << depth;
  cell = uint3(local % width, (local / width) % width, local / (width * width));
}
uint3 CornerBits(uint corner) {
  return uint3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
}
uint PointIndex(int3 lattice) {
  return uint(lattice.x) + CORNERS * (uint(lattice.y) + CORNERS * uint(lattice.z));
}
int3 PointCoordinates(uint index) {
  return int3(index % CORNERS, (index / CORNERS) % CORNERS, index / (CORNERS * CORNERS));
}
uint CellIndex(uint3 cell) {
  return cell.x + CELLS * (cell.y + CELLS * cell.z);
}
// PCG, one step. Hashing rather than carrying a generator, because every thread needs its own stream
// and the only state each one has is where it is and which frame this is.
uint Hash(uint value) {
  uint state = value * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}
float Random(inout uint state) {
  state = Hash(state);
  return float(state >> 8) / 16777216.0;
}
// Whether some leaf has this lattice point as one of its corners, and so needs a probe standing there.
//
// Read off the lookup grid rather than the tree: the eight finest cells around a point are the only
// places a leaf with that corner can be, and the grid already says which leaf covers each. A leaf
// only has the point as a corner if the point is where its box ends on every axis; a coarse leaf
// whose face the point merely lies on does not, and the probe there belongs to the finer leaves on
// the other side.
//
// The allowance is how far the probe may walk off its corner, and it is the smallest any of its
// leaves would grant -- see `PROBE_RELOCATION_LIMIT`.
bool Needed(int3 lattice, out float allowance) {
  bool needed = false;
  allowance = 3.402823e38;
  for (uint around = 0; around < 8; ++around) {
    int3 cell = lattice - int3(CornerBits(around));
    if (any(cell < 0) || any(cell >= int(CELLS)))
      continue;
    uint leaf = g_lookup[CellIndex(uint3(cell))];
    if (leaf >= NODE_COUNT)
      continue;
    uint depth;
    uint3 at;
    DecodeNode(leaf, depth, at);
    int stride = int(1u << (MAX_DEPTH - depth));
    int3 low = int3(at) * stride;
    int3 high = low + stride;
    if (!all(or(lattice == low, lattice == high)))
      continue;
    needed = true;
    allowance = min(allowance, u_tuning.z * g_nodes[leaf].extent);
  }
  return needed;
}
float3 PointPosition(int3 lattice) {
  return u_volume.xyz + float3(lattice) * (u_volume.w / float(CELLS));
}
bool ChildrenAreLeaves(uint depth, uint3 cell) {
  for (uint child = 0; child < 8; ++child)
    if (g_nodes[NodeIndex(depth + 1, cell * 2 + CornerBits(child))].leaf != STATE_LEAF)
      return false;
  return true;
}
// Lays out the complete tree, empties the lattice and hands every probe slot to the free list.
//
// The tree it leaves is the uniform part of the volume: every node above `u_counts.y` interior, every
// node at it a leaf, everything below absent. That is a usable volume from the first frame, and the
// adaptive half grows on top of it as the measurements come in. Nothing is measured yet, so every
// node starts unsampled, which is what puts all of them in the queue.
//
// Runs when the volume is first laid down and when it has to move -- a lattice in a new place has no
// probe that could be matched to an old one, so nothing would be gained by carrying anything across.
[numthreads(GROUP_SIZE, 1, 1)]
void CSReset(uint id : SV_DispatchThreadID) {
  if (id < NODE_COUNT) {
    uint depth;
    uint3 cell;
    DecodeNode(id, depth, cell);
    float size = u_volume.w / float(1u << depth);
    OctreeNode node;
    node.center = u_volume.xyz + (float3(cell) + 0.5) * size;
    node.extent = 0.5 * size;
    for (uint child = 0; child < 8; ++child) {
      node.children[child >> 2][child & 3] = depth < MAX_DEPTH ? NodeIndex(depth + 1, cell * 2 + CornerBits(child)) : INVALID_INDEX;
      node.probes[child >> 2][child & 3] = INVALID_INDEX;
    }
    node.depth = depth;
    node.leaf = depth < u_counts.y ? STATE_INTERIOR : depth == u_counts.y ? STATE_LEAF : STATE_ABSENT;
    node.surface = 0.0;
    node.samples = 0;
    g_nodes[id] = node;
  }
  if (id < POINTS)
    g_corners[id] = INVALID_INDEX;
  if (id < u_counts.x) {
    // Popped from the top, so slot nought goes first and the probes in use stay packed at the front.
    g_free[id] = u_counts.x - 1 - id;
    g_probes[id].position = 0.0;
    g_probes[id].anchor = 0.0;
  }
  if (id == 0) {
    g_counters[COUNTER_FREE_TOP] = int(u_counts.x);
    for (uint counter = 1; counter < 8; ++counter)
      g_counters[counter] = 0;
  }
}
// Zeroes what the passes below count afresh each frame. The free list's height is not one of them.
[numthreads(1, 1, 1)]
void CSBeginFrame() {
  for (uint counter = 1; counter < 8; ++counter)
    g_counters[counter] = 0;
}
// Forgets the measurement of every node a changed box touches, which is what puts it back in the queue.
//
// The thesis's AABB buffer, and what keeps the cost of a change proportional to its size. A clock
// digit swapping its mesh reaches the handful of nodes around it; the rest of the volume never hears
// about it, and its measurements stand. Interior nodes are forgotten too, since an interior node's
// measurement is what decides whether it should merge back into a leaf.
[numthreads(GROUP_SIZE, 1, 1)]
void CSDirty(uint id : SV_DispatchThreadID) {
  if (id >= NODE_COUNT || g_nodes[id].leaf == STATE_ABSENT)
    return;
  float3 low = g_nodes[id].center - g_nodes[id].extent;
  float3 high = g_nodes[id].center + g_nodes[id].extent;
  for (uint box = 0; box < u_counts.z; ++box) {
    if (any(g_boxes[box * 2].xyz > high) || any(g_boxes[box * 2 + 1].xyz < low))
      continue;
    g_nodes[id].samples = 0;
    g_nodes[id].surface = 0.0;
    return;
  }
}
// Gathers the nodes that have a decision outstanding and have not yet been measured enough to make it.
//
// A leaf has the decision to split, unless it is already as deep as the tree goes. An interior node
// has the decision to merge, but only when all eight of its children are leaves: anything further
// down has to merge first, one level at a time. Neither applies above the uniform depth, which is a
// floor rather than something the measurements are allowed to undo.
//
// Capped at `u_limits.y` a frame. What does not fit waits for the nodes ahead of it to settle and
// leave the queue, which on a scene that has just loaded means the tree fills in over a second or so
// rather than at once -- the uniform grid underneath is a usable volume in the meantime.
[numthreads(GROUP_SIZE, 1, 1)]
void CSQueue(uint id : SV_DispatchThreadID) {
  if (id >= NODE_COUNT)
    return;
  OctreeNode node = g_nodes[id];
  if (node.samples >= u_limits.z || node.depth < u_counts.y || node.depth >= MAX_DEPTH)
    return;
  bool candidate = node.leaf == STATE_LEAF;
  if (node.leaf == STATE_INTERIOR) {
    uint depth;
    uint3 cell;
    DecodeNode(id, depth, cell);
    candidate = ChildrenAreLeaves(depth, cell);
  }
  if (!candidate)
    return;
  int slot;
  InterlockedAdd(g_counters[COUNTER_QUEUED], 1, slot);
  if (uint(slot) < u_limits.y)
    g_queue[slot] = id;
}
// Casts random lines through each queued node and counts every surface each one crosses.
//
// The thesis decides on a hit fraction: the share of a probe's surfels that land on geometry inside
// the node. That saturates. One wall through a cell is hit by about a third of the lines, a corner
// where three walls meet by nearly all of them, and anything busier than a corner reads the same as
// the corner -- which is exactly the range where a split starts to be worth having.
//
// Counting crossings rather than first hits does not saturate, and it measures something with a
// name. For lines that are uniformly random in position and direction, the mean number of times a
// line meeting a convex body crosses a surface inside it is twice the surface's area over the body's
// own surface area -- Crofton's formula, the same one that makes a cube's own boundary come out at
// exactly two. A cube of side s has 6 s^2 of boundary, so the area the node holds is 3 s^2 times the
// mean count, and the test `PROBE_OCTREE_LEAF_SURFACE` was tuned on, area against the cell's own
// face, is three times the mean. The threshold carried over from the CPU build unchanged.
//
// A line is drawn by picking a direction, then a point uniformly on the disc through the centre that
// the bounding sphere projects to, and keeping it only if it actually meets the cube. That is what
// makes the lines uniform over the ones meeting the cube rather than biased towards long chords, as
// starting each at a random point inside would be. About two in three survive.
//
// The ray flag forces every triangle to report as a candidate and none is ever committed, so the
// traversal walks the whole chord and offers up each crossing on the way. Alpha is not consulted: a
// leaf of foliage is surface for this purpose whether or not a texel of it is cut away.
//
// Folded into a running mean on the node, so a node is judged on everything measured since it was
// last disturbed rather than on one frame's worth of lines.
[numthreads(GROUP_SIZE, 1, 1)]
void CSMeasure(uint3 group : SV_GroupID, uint thread : SV_GroupIndex) {
  uint queued = min(uint(max(g_counters[COUNTER_QUEUED], 0)), u_limits.y);
  if (group.x >= queued)
    return;
  uint nodeIndex = g_queue[group.x];
  float3 center = g_nodes[nodeIndex].center;
  float extent = g_nodes[nodeIndex].extent;
  float radius = extent * sqrt(3.0);
  uint state = Hash(nodeIndex ^ Hash(thread + GROUP_SIZE * u_counts.w));
  uint lines = 0;
  uint crossings = 0;
  for (uint cast = 0; cast < LINES_PER_THREAD; ++cast) {
    float z = 1.0 - 2.0 * Random(state);
    float phi = 2.0 * PI * Random(state);
    float planar = sqrt(saturate(1.0 - z * z));
    float3 direction = float3(cos(phi) * planar, sin(phi) * planar, z);
    float3 helper = abs(direction.x) < 0.9 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 tangent = normalize(cross(direction, helper));
    float3 bitangent = cross(direction, tangent);
    float r = radius * sqrt(Random(state));
    float theta = 2.0 * PI * Random(state);
    float3 through = center + (tangent * cos(theta) + bitangent * sin(theta)) * r;
    float3 safe = float3(abs(direction.x) > 1e-8 ? direction.x : 1e-8, abs(direction.y) > 1e-8 ? direction.y : 1e-8, abs(direction.z) > 1e-8 ? direction.z : 1e-8);
    float3 near = (center - extent - through) / safe;
    float3 far = (center + extent - through) / safe;
    float enter = max(max(min(near.x, far.x), min(near.y, far.y)), min(near.z, far.z));
    float leave = min(min(max(near.x, far.x), max(near.y, far.y)), max(near.z, far.z));
    if (leave <= enter)
      continue;
    RayDesc ray;
    ray.Origin = through + direction * enter;
    ray.Direction = direction;
    ray.TMin = 0.0;
    ray.TMax = leave - enter;
    RayQuery<RAY_FLAG_FORCE_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
    query.TraceRayInline(g_scene, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed())
      if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        ++crossings;
    ++lines;
  }
  gs_lines[thread] = lines;
  gs_crossings[thread] = crossings;
  GroupMemoryBarrierWithGroupSync();
  if (thread != 0)
    return;
  uint frameLines = 0;
  uint frameCrossings = 0;
  for (uint other = 0; other < GROUP_SIZE; ++other) {
    frameLines += gs_lines[other];
    frameCrossings += gs_crossings[other];
  }
  uint previous = g_nodes[nodeIndex].samples;
  uint total = previous + frameLines;
  if (total == 0)
    return;
  g_nodes[nodeIndex].surface = (g_nodes[nodeIndex].surface * float(previous) + float(frameCrossings)) / float(total);
  g_nodes[nodeIndex].samples = total;
}
// The area a node holds against its own face, from its mean crossing count. See `CSMeasure`.
float SurfaceRatio(OctreeNode node) {
  return 3.0 * node.surface;
}
// Folds back every interior node whose eight leaves turned out to hold little enough.
//
// Before the splits rather than beside them, so a node and its child are never decided on in the same
// dispatch: a node merging here takes its children out of the tree, and the split pass then finds
// them absent rather than racing to split one of them.
//
// The hysteresis is what keeps a node near the threshold from splitting one frame and merging the
// next. Splitting asks for more than the threshold and merging for less, so a measurement has to move
// by the whole gap before the decision it made is undone.
[numthreads(GROUP_SIZE, 1, 1)]
void CSDecideMerge(uint id : SV_DispatchThreadID) {
  if (id >= NODE_COUNT)
    return;
  OctreeNode node = g_nodes[id];
  if (node.leaf != STATE_INTERIOR || node.depth < u_counts.y || node.depth >= MAX_DEPTH || node.samples < u_limits.z)
    return;
  if (SurfaceRatio(node) >= u_tuning.x * (1.0 - u_tuning.y))
    return;
  uint depth;
  uint3 cell;
  DecodeNode(id, depth, cell);
  if (!ChildrenAreLeaves(depth, cell))
    return;
  for (uint child = 0; child < 8; ++child)
    g_nodes[NodeIndex(depth + 1, cell * 2 + CornerBits(child))].leaf = STATE_ABSENT;
  // Keeps its measurement, which is what it was merged on, so it does not go straight back into the
  // queue to be split again on the strength of the same lines.
  g_nodes[id].leaf = STATE_LEAF;
  InterlockedAdd(g_counters[COUNTER_MERGES], 1);
}
// Splits every leaf holding more surface than the threshold allows, one level at a time.
//
// The eight new leaves start unmeasured and join the queue, so a region that needs to go two levels
// deeper gets there over two rounds of measuring rather than at once -- each level judged on lines
// through its own cells, which is the only way to know the second split is wanted.
[numthreads(GROUP_SIZE, 1, 1)]
void CSDecideSplit(uint id : SV_DispatchThreadID) {
  if (id >= NODE_COUNT)
    return;
  OctreeNode node = g_nodes[id];
  if (node.leaf != STATE_LEAF || node.depth < u_counts.y || node.depth >= MAX_DEPTH || node.samples < u_limits.z)
    return;
  if (SurfaceRatio(node) <= u_tuning.x * (1.0 + u_tuning.y))
    return;
  uint depth;
  uint3 cell;
  DecodeNode(id, depth, cell);
  for (uint child = 0; child < 8; ++child) {
    uint index = NodeIndex(depth + 1, cell * 2 + CornerBits(child));
    g_nodes[index].leaf = STATE_LEAF;
    g_nodes[index].samples = 0;
    g_nodes[index].surface = 0.0;
  }
  g_nodes[id].leaf = STATE_INTERIOR;
  InterlockedAdd(g_counters[COUNTER_SPLITS], 1);
}
// Points each cell of the lookup grid at the leaf covering it.
//
// Rewritten whole every frame rather than patched where the tree changed. It is 32768 threads that
// each read at most six states, which is cheaper than working out which cells a frame's splits and
// merges reached.
[numthreads(GROUP_SIZE, 1, 1)]
void CSLookup(uint id : SV_DispatchThreadID) {
  if (id >= CELLS * CELLS * CELLS)
    return;
  uint3 cell = uint3(id % CELLS, (id / CELLS) % CELLS, id / (CELLS * CELLS));
  uint found = INVALID_INDEX;
  for (uint depth = 0; depth <= MAX_DEPTH; ++depth) {
    uint index = NodeIndex(depth, cell >> (MAX_DEPTH - depth));
    uint state = g_nodes[index].leaf;
    if (state == STATE_LEAF) {
      found = index;
      break;
    }
    if (state == STATE_ABSENT)
      break;
  }
  g_lookup[id] = found;
}
// Hands back the probe at every lattice point no leaf has as a corner any more.
//
// Its own dispatch, ahead of the one that allocates, so a slot is never pushed and popped by the same
// dispatch. The stack is then only ever growing in this pass and only ever shrinking in the next,
// which is what lets both be a single atomic on its height.
[numthreads(GROUP_SIZE, 1, 1)]
void CSRelease(uint id : SV_DispatchThreadID) {
  if (id >= POINTS)
    return;
  uint slot = g_corners[id];
  float allowance;
  if (slot == INVALID_INDEX || Needed(PointCoordinates(id), allowance))
    return;
  // An allowance of nought is what marks a slot as holding no probe, for the trace and the debug view.
  g_probes[slot].position = 0.0;
  g_probes[slot].anchor = 0.0;
  int top;
  InterlockedAdd(g_counters[COUNTER_FREE_TOP], 1, top);
  g_free[top] = slot;
  g_corners[id] = INVALID_INDEX;
}
// Gives a probe to every lattice point a leaf has newly come to need one at.
//
// A pop that finds the stack empty puts its decrement back and is counted as a failure. The C++
// reads that count back a few frames later and grows the pool; until then the point simply has no
// probe, shading skips the missing corner, and this pass tries again every frame.
//
// A new probe is marked fresh and left for the seed pass to fill in, rather than started from black.
[numthreads(GROUP_SIZE, 1, 1)]
void CSAcquire(uint id : SV_DispatchThreadID) {
  if (id >= POINTS || g_corners[id] != INVALID_INDEX)
    return;
  int3 lattice = PointCoordinates(id);
  float allowance;
  if (!Needed(lattice, allowance))
    return;
  int top;
  InterlockedAdd(g_counters[COUNTER_FREE_TOP], -1, top);
  if (top <= 0) {
    InterlockedAdd(g_counters[COUNTER_FREE_TOP], 1);
    InterlockedAdd(g_counters[COUNTER_FAILED], 1);
    return;
  }
  uint slot = g_free[top - 1];
  float3 anchor = PointPosition(lattice);
  g_probes[slot].position = float4(anchor, FRESH);
  g_probes[slot].anchor = float4(anchor, allowance);
  for (uint coefficient = 0; coefficient < 9; ++coefficient)
    g_probes[slot].irradiance[coefficient] = 0.0;
  // Seeded as though the scene were as far away as the clamp allows, and with no spread, so a fresh
  // probe is visible from everywhere until its own rays say otherwise. Zero would read as a surface
  // at no distance at all, which is total occlusion.
  uint seed = f32tof16(min(u_tuning.w, 65504.0));
  for (uint texel = 0; texel < 256; ++texel)
    g_probes[slot].depth[texel] = seed;
  for (uint word = 0; word < 64; ++word)
    g_probes[slot].behind[word] = 0;
  for (uint face = 0; face < 6; ++face)
    g_probes[slot].neighbours[face] = INVALID_INDEX;
  g_probes[slot].exterior = 0.0;
  g_corners[id] = slot;
}
// Starts each fresh probe from what the field already says about where it stands.
//
// The CPU build had to read the whole field back to do this, and only managed it for probes standing
// exactly where an old one had. Here every new probe is one a split has just put between probes that
// were already there -- on an edge, a face or the middle of the leaf that split -- so the field at its
// position is a trilinear blend of the corners of the coarser cell around it. It is seeded with that
// blend and converges from there, rather than appearing black and being visible as it brightens.
//
// Only probes that were not fresh are read from. A fresh probe's neighbours may be fresh too and be
// written by this same dispatch; reading them would be reading something half set up. The coarsest
// cell is tried last, and a probe with no settled corners at any size -- every probe, just after a
// reset -- keeps the black it was allocated with, as the CPU build's probes all did.
//
// Trust is carried in `position.w` encoded below nought, so the probe still reads as unfinished to
// every other thread in this pass; `CSRefresh` decodes it. Visibility is not blended: a depth map is a
// set of distances from one particular point, and an average of four of them is a distance from
// nowhere.
[numthreads(GROUP_SIZE, 1, 1)]
void CSSeed(uint id : SV_DispatchThreadID) {
  if (id >= POINTS)
    return;
  uint slot = g_corners[id];
  if (slot == INVALID_INDEX || g_probes[slot].position.w != FRESH)
    return;
  int3 lattice = PointCoordinates(id);
  for (uint level = 1; level <= MAX_DEPTH; ++level) {
    int span = int(1u << level);
    int3 base = min((lattice >> level) << level, int(CELLS) - span);
    float3 t = float3(lattice - base) / float(span);
    float3 irradiance[9];
    for (uint coefficient = 0; coefficient < 9; ++coefficient)
      irradiance[coefficient] = 0.0;
    float total = 0.0;
    float trust = 0.0;
    float exterior = 0.0;
    for (uint corner = 0; corner < 8; ++corner) {
      uint3 bits = CornerBits(corner);
      uint source = g_corners[PointIndex(base + int3(bits) * span)];
      if (source == INVALID_INDEX)
        continue;
      float believed = g_probes[source].position.w;
      if (believed < 0.0)
        continue;
      float3 axis = select(bits != 0, t, 1.0 - t);
      float weight = axis.x * axis.y * axis.z;
      if (weight <= 0.0)
        continue;
      for (uint coefficient = 0; coefficient < 9; ++coefficient)
        irradiance[coefficient] += g_probes[source].irradiance[coefficient].rgb * weight;
      trust += believed * weight;
      exterior += g_probes[source].exterior * weight;
      total += weight;
    }
    // Most of the blend has to be there, not merely some of it: a probe seeded from one corner of
    // eight is that corner's value moved to a place it does not describe.
    if (total <= 0.5)
      continue;
    for (uint coefficient = 0; coefficient < 9; ++coefficient)
      g_probes[slot].irradiance[coefficient] = float4(irradiance[coefficient] / total, 0.0);
    g_probes[slot].exterior = exterior / total;
    g_probes[slot].position.w = -(2.0 + trust / total);
    return;
  }
}
// Brings every probe's allowance and neighbours up to date with the tree, and finishes fresh ones.
//
// Both depend on the leaves around a probe, which any split or merge nearby can change. Recomputing
// them for every probe every frame costs one thread per lattice point and a few dozen reads, which is
// less than keeping track of which ones a frame's changes reached.
//
// A neighbour is the nearest probe along each axis, stepping outward until one is found and giving up
// past the stride of the coarsest leaf allowed, which is the furthest a genuine neighbour can be.
[numthreads(GROUP_SIZE, 1, 1)]
void CSRefresh(uint id : SV_DispatchThreadID) {
  if (id >= POINTS)
    return;
  uint slot = g_corners[id];
  if (slot == INVALID_INDEX)
    return;
  int3 lattice = PointCoordinates(id);
  float allowance;
  if (Needed(lattice, allowance))
    g_probes[slot].anchor.w = allowance;
  float believed = g_probes[slot].position.w;
  if (believed < 0.0)
    g_probes[slot].position.w = believed == FRESH ? 0.0 : -believed - 2.0;
  for (uint face = 0; face < 6; ++face) {
    uint axis = face >> 1;
    int step = (face & 1) ? 1 : -1;
    uint neighbour = INVALID_INDEX;
    for (uint distance = 1; distance <= u_limits.w; ++distance) {
      int3 at = lattice;
      at[axis] += step * int(distance);
      if (at[axis] < 0 || at[axis] >= int(CORNERS))
        break;
      neighbour = g_corners[PointIndex(at)];
      if (neighbour != INVALID_INDEX)
        break;
    }
    g_probes[slot].neighbours[face] = neighbour;
  }
  InterlockedAdd(g_counters[COUNTER_PROBES], 1);
}
// Writes each leaf's eight corner probes into it, which is the form shading reads them in.
[numthreads(GROUP_SIZE, 1, 1)]
void CSLeafProbes(uint id : SV_DispatchThreadID) {
  if (id >= NODE_COUNT)
    return;
  bool leaf = g_nodes[id].leaf == STATE_LEAF;
  uint depth;
  uint3 cell;
  DecodeNode(id, depth, cell);
  int stride = int(1u << (MAX_DEPTH - depth));
  uint4 probes[2];
  for (uint corner = 0; corner < 8; ++corner)
    probes[corner >> 2][corner & 3] = leaf ? g_corners[PointIndex((int3(cell) + int3(CornerBits(corner))) * stride)] : INVALID_INDEX;
  g_nodes[id].probes[0] = probes[0];
  g_nodes[id].probes[1] = probes[1];
  if (!leaf)
    return;
  InterlockedAdd(g_counters[COUNTER_LEAVES], 1);
  InterlockedMax(g_counters[COUNTER_DEEPEST], int(depth));
}
// Pushes the slots a growth of the pool added onto the free list, marked as holding nothing.
[numthreads(GROUP_SIZE, 1, 1)]
void CSExtendFree(uint id : SV_DispatchThreadID) {
  if (id >= u_counts.x - u_limits.x)
    return;
  uint slot = u_limits.x + id;
  g_probes[slot].position = 0.0;
  g_probes[slot].anchor = 0.0;
  int top;
  InterlockedAdd(g_counters[COUNTER_FREE_TOP], 1, top);
  g_free[top] = slot;
}
