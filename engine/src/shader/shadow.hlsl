struct InstanceData {
  float4x4 model;
  uint entityId;
  uint3 padding;
};
cbuffer ShadowConstants : register(b0) {
  float4x4 u_lightViewProjection;
};
StructuredBuffer<InstanceData> g_instances : register(t0);
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
