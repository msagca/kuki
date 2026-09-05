cbuffer VolumeConstants : register(b0) {
  float4 u_origin;
  uint4 u_grid;
};
struct OctreeNode {
  float3 center;
  float extent;
  uint4 children[2];
  uint4 probes[2];
  uint depth;
  uint leaf;
  uint2 padding;
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
StructuredBuffer<OctreeNode> g_nodes : register(t0);
StructuredBuffer<Probe> g_probes : register(t1);
StructuredBuffer<uint> g_lookup : register(t2);
RWStructuredBuffer<uint> g_result : register(u0);
float3 CornerOffset(uint corner, float extent) {
  return float3((corner & 1) ? extent : -extent, (corner & 2) ? extent : -extent, (corner & 4) ? extent : -extent);
}
[numthreads(4, 4, 4)]
void CSAudit(uint3 id : SV_DispatchThreadID) {
  uint audit = u_grid.y;
  if (any(id >= audit))
    return;
  float side = u_origin.w;
  float3 origin = u_origin.xyz;
  float3 world = origin + (float3(id) + 0.5) / float(audit) * side;
  float epsilon = side * 1e-4;
  uint resolution = u_grid.x;
  uint3 cell = min(uint3((world - origin) / side * float(resolution)), resolution - 1);
  uint leafIndex = g_lookup[cell.x + resolution * (cell.y + resolution * cell.z)];
  if (leafIndex >= u_grid.w) {
    InterlockedAdd(g_result[1], 1);
    return;
  }
  OctreeNode node = g_nodes[leafIndex];
  if (node.leaf == 0) {
    InterlockedAdd(g_result[1], 1);
    return;
  }
  InterlockedMax(g_result[5], node.depth);
  InterlockedMin(g_result[6], node.depth);
  if (any(world < node.center - node.extent - epsilon) || any(world > node.center + node.extent + epsilon)) {
    InterlockedAdd(g_result[2], 1);
    return;
  }
  bool indexed = true;
  bool positioned = true;
  for (uint corner = 0; corner < 8; ++corner) {
    uint probe = node.probes[corner >> 2][corner & 3];
    if (probe >= u_grid.z) {
      indexed = false;
      continue;
    }
    float3 expected = node.center + CornerOffset(corner, node.extent);
    if (distance(g_probes[probe].anchor.xyz, expected) > epsilon)
      positioned = false;
  }
  if (!indexed)
    InterlockedAdd(g_result[3], 1);
  if (!positioned)
    InterlockedAdd(g_result[4], 1);
  if (indexed && positioned)
    InterlockedAdd(g_result[7], 1);
}
