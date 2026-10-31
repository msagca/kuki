cbuffer ShadowConstants : register(b0) {
  float4x4 u_lightViewProjection;
  // The scene's wind, as scene.hlsl has it. See `Displace`.
  float4 u_wind;
  float4 u_gust;
  float4 u_waves;
  // The pool being drawn: how far its material bends in the wind (`x`) and heaves with the waves
  // (`y`). Set per draw, since a depth pass draws pool after pool under one light.
  float4 u_draw;
};
// A pool's world matrices, one per entity, as they lie in the pool's column.
StructuredBuffer<float4x4> g_instances : register(t0);
// The skinned meshes' bones, already carried into world space -- the same buffer the scene pass is
// handed for the same draw. Read only by `VSSkinned`.
StructuredBuffer<float4x4> g_bones : register(t1);
struct VSInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texture0 : TEXCOORD0;
  float3 tangent : TANGENT;
};
// Where the scene's wind and waves move a vertex, in the world. The same function, word for word,
// is in scene.hlsl, and has to stay so: a surface the depth passes put somewhere else from where
// this pass draws it casts a shadow that slides off it and is inked along an edge it no longer has.
//
// `u_wind` is the wind's direction along the ground, its strength and the clock; `u_gust` the
// gusts' strength, spacing and speed and the shiver's rate; `u_waves` the shiver's amount and the
// waves' height, length and speed. See `Wind`.
float WaveHeight(float2 p) {
  float2 d1 = u_wind.xy;
  float2 d2 = float2(d1.x * 0.825 - d1.y * 0.565, d1.x * 0.565 + d1.y * 0.825);
  float2 d3 = float2(d1.x * 0.622 + d1.y * 0.783, -d1.x * 0.783 + d1.y * 0.622);
  float k = 6.2831853 / u_waves.z;
  float travel = u_wind.w * u_waves.w;
  // Three trains, the longest with the wind and two shorter across it, at lengths no two of which
  // divide one another: the sum never repeats within sight, and never shows one wave marching.
  float h = sin((dot(p, d1) - travel) * k) * 0.55;
  h += sin((dot(p, d2) - travel * 0.8) * k * 1.73 + 1.3) * 0.3;
  h += sin((dot(p, d3) - travel * 0.7) * k * 2.91 + 2.1) * 0.15;
  return h * u_waves.y * 0.5;
}
float3 Displace(float3 world, float3 origin, float sway, float waves) {
  if (sway > 0.0 && u_wind.z > 0.0) {
    float2 dir = u_wind.xy;
    float t = u_wind.w;
    float height = max(world.y - origin.y, 0.0);
    float bend = sway * height * height;
    // Bands of gust rolling along the wind, read off where the thing stands rather than the vertex,
    // so a tree leans as one and a field shows the band crossing it tree by tree.
    float along = dot(origin.xz, dir);
    float gust = pow(saturate(0.5 + 0.5 * sin((along - t * u_gust.z) / u_gust.y * 6.2831853)), 3.0) * u_gust.x;
    float lean = u_wind.z * (1.0 + gust);
    // Out of step from one thing to the next by a hash of where it stands.
    float seed = frac(sin(dot(origin.xz, float2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    float shiver = sin(t * u_gust.w * 6.2831853 + seed + height * 0.7) * u_waves.x * lean;
    float sideways = cos(t * u_gust.w * 4.37 + seed * 1.7) * u_waves.x * lean * 0.5;
    float2 push = (dir * (lean + shiver) + float2(-dir.y, dir.x) * sideways) * bend;
    world.xz += push;
    // Lowered by about what bending through that arc takes off its height, so a tall thing bowing
    // far does not stretch.
    world.y -= dot(push, push) / max(2.0 * height, 0.05);
  }
  if (waves > 0.0 && u_waves.y > 0.0)
    world.y += waves * WaveHeight(world.xz);
  return world;
}
float4 VSMain(VSInput input, uint instance : SV_InstanceID) : SV_POSITION {
  float4x4 model = g_instances[instance];
  float4 worldPosition = mul(model, float4(input.position, 1.0));
  worldPosition.xyz = Displace(worldPosition.xyz, mul(model, float4(0.0, 0.0, 0.0, 1.0)).xyz, u_draw.x, u_draw.y);
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
    world = g_instances[instance];
  return mul(u_lightViewProjection, mul(world, float4(input.position, 1.0)));
}
