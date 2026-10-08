cbuffer SkyboxConstants : register(b0) {
  float4x4 u_inverseViewProjection;
  uint u_useTexture;
  uint u_useGradient;
  uint2 u_padding;
  float4 u_background;
  float3 u_sunDirection;
  float u_sunCosRadius;
  float3 u_sunRadiance;
  uint u_useSunDisc;
};
TextureCube g_skybox : register(t0);
Texture2D g_sunTransmittance : register(t1);
SamplerState g_sampler : register(s0);
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
    output.color = float4(u_background.rgb, 1.0);
  // Added over whatever the sky turned out to be, rather than replacing it, so the disc sits in
  // front of the air between it and the viewer instead of cutting a hole through it.
  //
  // Only where an atmosphere computed the sky. A photographed one already has a sun somewhere in
  // the image, and drawing a second one over it would be a sun beside a sun.
  if (u_useSunDisc != 0) {
    float cosAngle = dot(direction, u_sunDirection);
    // Softened across a band rather than cut off at the edge. The disc is a quarter of a degree
    // across and brighter than everything near it by some orders of magnitude, so a hard step would
    // crawl along its rim as the camera turns, and the multisampling only has four samples to
    // spend on it. The band is a twentieth of the radius, which is below what can be seen and far
    // more than enough to settle the edge.
    float edge = (1.0 - u_sunCosRadius) * 0.05;
    float disc = smoothstep(u_sunCosRadius - edge, u_sunCosRadius + edge, cosAngle);
    output.color.rgb += u_sunRadiance * g_sunTransmittance.SampleLevel(g_sampler, float2(0.5, 0.5), 0.0).rgb * disc;
  }
  output.entityId = ENTITY_ID_INVALID;
  return output;
}
