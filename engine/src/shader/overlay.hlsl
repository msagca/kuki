// Text drawn over the finished picture, in the window's own pixels. The counterpart of
// `overlay.vert` and `overlay.frag`; see those for the shape, which is the same on both backends.
struct OverlayConstants {
  float4x4 projection;
  float4 color;
};
ConstantBuffer<OverlayConstants> constants : register(b0);
Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);
// Only the two attributes the text needs. The buffer is laid out as `Vertex` like any other mesh,
// so the stride carries a normal and a tangent that the input layout simply does not name.
struct VSInput {
  float3 position : POSITION;
  float2 texCoords : TEXCOORD0;
};
struct PSInput {
  float4 position : SV_POSITION;
  float2 texCoords : TEXCOORD0;
};
PSInput VSMain(VSInput input) {
  PSInput output;
  output.position = mul(constants.projection, float4(input.position, 1.0));
  output.texCoords = input.texCoords;
  return output;
}
float4 PSMain(PSInput input) : SV_TARGET {
  // Only the alpha is read, exactly as the GLSL does: the atlas is white everywhere and carries
  // the glyph in its fourth channel, so the colour is entirely the caller's.
  float coverage = atlas.Sample(atlasSampler, input.texCoords).a;
  return float4(constants.color.rgb, constants.color.a * coverage);
}
