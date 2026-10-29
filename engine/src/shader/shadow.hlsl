struct InstanceData {
  float4x4 model;
  uint entityId;
  uint3 padding;
};
cbuffer ShadowConstants : register(b0) {
  float4x4 u_lightViewProjection;
};
StructuredBuffer<InstanceData> g_instances : register(t0);
// The skinned meshes' bones, already carried into world space -- the same buffer the scene pass is
// handed for the same draw. Read only by `VSSkinned`.
StructuredBuffer<float4x4> g_bones : register(t1);
struct VSInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texture0 : TEXCOORD0;
  float3 tangent : TANGENT;
};
float4 VSMain(VSInput input, uint instance : SV_InstanceID) : SV_POSITION {
  float4 worldPosition = mul(g_instances[instance].model, float4(input.position, 1.0));
  return mul(u_lightViewProjection, worldPosition);
}
struct VSSkinnedInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texture0 : TEXCOORD0;
  float3 tangent : TANGENT;
  int4 boneIds : BLENDINDICES;
  float4 boneWeights : BLENDWEIGHT;
};
// A figure's depth, posed as the scene pass poses it. Without this a skinned mesh was in neither the
// shadow map nor the depth prepass: it cast no shadow, and everything that reads the prepass -- the
// particles, the fog, the ink -- saw straight through it. Weighted exactly as `scene.hlsl` weights
// it, so the silhouette in the map is the silhouette on the screen.
float4 VSSkinned(VSSkinnedInput input, uint instance : SV_InstanceID) : SV_POSITION {
  float4x4 world = (float4x4)0;
  float total = 0.0;
  [unroll] for (int i = 0; i < 4; ++i)
    if (input.boneIds[i] >= 0 && input.boneWeights[i] > 0.0) {
      world += g_bones[input.boneIds[i]] * input.boneWeights[i];
      total += input.boneWeights[i];
    }
  if (total <= 0.0)
    world = g_instances[instance].model;
  return mul(u_lightViewProjection, mul(world, float4(input.position, 1.0)));
}
