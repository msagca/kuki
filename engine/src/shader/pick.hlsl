// One texel of the multisampled entity id buffer, read without resolving it.
//
// A resolve averages the samples it covers, and the average of two ids is a third id that belongs
// to neither of them -- so every pixel on the silhouette of an object used to answer a click with an
// entity that is not in the scene. Loading the samples instead means every value considered here is
// one a triangle actually wrote, and the answer is always something the pixel really shows.
cbuffer PickConstants : register(b0) {
  /// @brief The texel to read in `xy`, and how many samples it carries in `z`.
  uint4 u_pick;
};
Texture2DMS<float4> g_ids : register(t0);
RWStructuredBuffer<uint> g_result : register(u0);
static const uint ENTITY_ID_INVALID = 0xFFFFFF;
uint DecodeId(float4 encoded) {
  return uint(encoded.r * 255.0 + 0.5) | (uint(encoded.g * 255.0 + 0.5) << 8) | (uint(encoded.b * 255.0 + 0.5) << 16);
}
[numthreads(1, 1, 1)]
void CSPick(uint3 thread : SV_DispatchThreadID) {
  const int2 coord = int2(u_pick.xy);
  // Sample 0 first, because that is the one the outline pass draws from: a click and the highlight
  // it produces should agree about what is under the cursor. The rest are only consulted when
  // sample 0 is background, which is what lets a click land on a piece from a pixel the piece
  // covers by a quarter -- generous at the silhouette, and never generous towards nothing.
  uint id = DecodeId(g_ids.Load(coord, 0));
  for (uint sample = 1; sample < u_pick.z && id == ENTITY_ID_INVALID; ++sample)
    id = DecodeId(g_ids.Load(coord, int(sample)));
  g_result[0] = id;
}
