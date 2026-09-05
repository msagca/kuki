cbuffer ProbeConstants : register(b0) {
  float4x4 u_inverseViewProjection;
  float4 u_origin;
  uint4 u_dimensions;
};
struct GeometryInfo {
  uint vertexBuffer;
  uint indexBuffer;
  uint materialTextures;
  uint textureMask;
  float4 albedo;
  float4 emissive;
  float4 attenuation;
  float transmission;
  float thickness;
  float alphaCutoff;
  uint alphaMode;
  uint entityId;
  uint3 padding;
};
static const uint INVALID_INDEX = 0xFFFFFFFF;
static const float DISTANCE_SCALE = 1000.0;
static const float RAY_MIN = 0.001;
static const float RAY_MAX = 100000.0;
RaytracingAccelerationStructure g_scene : register(t0);
StructuredBuffer<GeometryInfo> g_geometry : register(t1);
RWStructuredBuffer<uint> g_result : register(u0);
[numthreads(8, 8, 1)]
void CSProbe(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_dimensions.x || id.y >= u_dimensions.y)
    return;
  float2 uv = (float2(id.xy) + 0.5) / float2(u_dimensions.xy);
  float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  float4 unprojected = mul(u_inverseViewProjection, float4(ndc, 1.0, 1.0));
  RayDesc ray;
  ray.Origin = u_origin.xyz;
  ray.Direction = normalize(unprojected.xyz / unprojected.w - u_origin.xyz);
  ray.TMin = RAY_MIN;
  ray.TMax = RAY_MAX;
  RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
  query.TraceRayInline(g_scene, RAY_FLAG_NONE, 0xFF, ray);
  query.Proceed();
  if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
    InterlockedAdd(g_result[1], 1);
    return;
  }
  uint scaled = uint(query.CommittedRayT() * DISTANCE_SCALE);
  InterlockedAdd(g_result[0], 1);
  InterlockedMin(g_result[2], scaled);
  InterlockedMax(g_result[3], scaled);
  GeometryInfo info = g_geometry[query.CommittedInstanceID()];
  InterlockedOr(g_result[4], 1u << (info.entityId & 31));
  if (info.vertexBuffer != INVALID_INDEX)
    InterlockedAdd(g_result[5], 1);
  if (info.materialTextures != INVALID_INDEX)
    InterlockedAdd(g_result[6], 1);
}
