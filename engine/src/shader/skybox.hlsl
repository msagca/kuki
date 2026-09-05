cbuffer SkyboxConstants : register(b0) {
  float4x4 u_inverseViewProjection;
  uint u_useTexture;
  uint u_useGradient;
};
TextureCube g_skybox : register(t0);
SamplerState g_sampler : register(s0);
static const float4 COLOR_GRAY = float4(0.1, 0.1, 0.1, 1.0);
static const float4 ENTITY_ID_INVALID = float4(1.0, 1.0, 1.0, 1.0);
struct PSInput {
  float4 position : SV_POSITION;
  float2 ndc : TEXCOORD0;
};
PSInput VSMain(uint id : SV_VertexID) {
  PSInput output;
  float2 corner = float2((id << 1) & 2, id & 2);
  output.position = float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  output.ndc = output.position.xy;
  return output;
}
struct PSOutput {
  float4 color : SV_TARGET0;
  float4 entityId : SV_TARGET1;
};
PSOutput PSMain(PSInput input) {
  PSOutput output;
  float4 unprojected = mul(u_inverseViewProjection, float4(input.ndc, 1.0, 1.0));
  float3 direction = normalize(unprojected.xyz / unprojected.w);
  if (u_useTexture != 0)
    output.color = float4(g_skybox.SampleLevel(g_sampler, direction, 0.0).rgb, 1.0);
  else if (u_useGradient != 0) {
    float t = saturate(direction.y * 0.5 + 0.5);
    float3 horizon = float3(0.6, 0.7, 0.9);
    float3 zenith = float3(0.0, 0.1, 0.4);
    output.color = float4(lerp(horizon, zenith, t), 1.0);
  } else
    output.color = COLOR_GRAY;
  output.entityId = ENTITY_ID_INVALID;
  return output;
}
