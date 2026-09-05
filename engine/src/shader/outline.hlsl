cbuffer OutlineConstants : register(b0) {
  float4 u_color;
  float4 u_texelSize;
  uint4 u_selectedCount;
  uint4 u_selected[4];
};
Texture2D g_source : register(t0);
Texture2DMS<float4> g_ids : register(t1);
SamplerState g_sampler : register(s0);
static const uint ENTITY_ID_INVALID = 0xFFFFFF;
struct PSInput {
  float4 position : SV_POSITION;
  float2 texture0 : TEXCOORD0;
};
PSInput VSMain(uint id : SV_VertexID) {
  PSInput output;
  output.texture0 = float2((id << 1) & 2, id & 2);
  output.position = float4(output.texture0 * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
  return output;
}
uint LoadEntityId(int2 coord) {
  float4 encoded = g_ids.Load(coord, 0);
  return uint(encoded.r * 255.0 + 0.5) | (uint(encoded.g * 255.0 + 0.5) << 8) | (uint(encoded.b * 255.0 + 0.5) << 16);
}
bool IsSelected(uint id) {
  if (id == ENTITY_ID_INVALID)
    return false;
  uint count = min(u_selectedCount.x, 16u);
  for (uint i = 0; i < count; ++i)
    if (u_selected[i / 4][i % 4] == id)
      return true;
  return false;
}
float4 PSOutline(PSInput input) : SV_TARGET {
  float4 base = g_source.Sample(g_sampler, input.texture0);
  int2 coord = int2(input.position.xy);
  if (IsSelected(LoadEntityId(coord)))
    return base;
  int thickness = max(1, int(u_color.a));
  for (int dy = -thickness; dy <= thickness; ++dy)
    for (int dx = -thickness; dx <= thickness; ++dx)
      if (IsSelected(LoadEntityId(coord + int2(dx, dy))))
        return float4(u_color.rgb, base.a);
  return base;
}
