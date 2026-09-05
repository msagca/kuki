static const float PI = 3.14159265359;
// Texels across one axis of a probe's visibility map, which the octahedron unfolds into a square of.
// Mirrors PROBE_DEPTH_RESOLUTION in dx_probe_volume.hpp.
static const int PROBE_DEPTH_RESOLUTION = 16;
// What the probe's fields are shown as. Mirrors ProbeDebugView in debug_view.hpp, whose declaration
// order these follow.
static const uint PROBE_VIEW_IRRADIANCE = 1;
static const uint PROBE_VIEW_DISTANCE = 2;
static const uint PROBE_VIEW_VARIANCE = 3;
static const uint PROBE_VIEW_TRUST = 4;
static const uint PROBE_VIEW_RELOCATION = 5;
static const uint PROBE_VIEW_CLASSIFICATION = 6;
// Spread of distances, as a fraction of the volume's reach, that the variance view shows as white.
//
// The quantity itself is small -- a texel looking at one flat surface has almost none, and even one
// straddling an edge rarely spreads across more than a fraction of what the probe can see. Shown
// against the full range it would be a uniformly black field carrying the information in its bottom
// few percent, which is a view of nothing. A quarter puts the interesting part across the whole
// scale, at the cost of saturating where the spread is already past any doubt.
static const float PROBE_VARIANCE_SCALE = 0.25;
// Share of its allowance a probe must have walked before it counts as having been relocated.
//
// Relocation is damped, so a probe settles near where it wants to be rather than exactly there and
// nothing ever reads as having moved by precisely nought. This is the line between numerical
// wandering and a probe that really did have to escape something.
static const float PROBE_MOVED_LIMIT = 0.05;
// Trust below which a probe is being spoken for by its neighbours rather than heard.
//
// Trust runs from one, for a probe in open air, down towards nought as more of its rays come back
// off the far side of a surface. Halfway is well clear of both: a probe just inside a surface sits
// near a half of its sphere blocked, and one in open space sits at one.
static const float PROBE_TRUSTED_LIMIT = 0.5;
struct Probe {
  float4 position;
  float4 anchor;
  float4 irradiance[9];
  uint depth[256];
  uint behind[64];
  uint neighbours[6];
  float exterior;
};
cbuffer DebugConstants : register(b0) {
  float4x4 u_viewProjection;
  float u_scale;
  uint u_mode;
  float u_range;
  float u_padding;
};
StructuredBuffer<Probe> g_probes : register(t0);
struct VSInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
};
struct PSInput {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  nointerpolation uint probe : TEXCOORD0;
};
struct PSOutput {
  float4 color : SV_TARGET0;
  float4 entityId : SV_TARGET1;
};
PSInput VSMain(VSInput input, uint instance : SV_InstanceID) {
  PSInput output;
  float3 center = g_probes[instance].position.xyz;
  output.position = mul(u_viewProjection, float4(center + input.position * u_scale, 1.0));
  output.normal = input.normal;
  output.probe = instance;
  return output;
}
float3 EvaluateProbe(uint probe, float3 n) {
  const float c1 = 0.429043;
  const float c2 = 0.511664;
  const float c3 = 0.743125;
  const float c4 = 0.886227;
  const float c5 = 0.247708;
  float3 L00 = g_probes[probe].irradiance[0].rgb;
  float3 L1m1 = g_probes[probe].irradiance[1].rgb;
  float3 L10 = g_probes[probe].irradiance[2].rgb;
  float3 L11 = g_probes[probe].irradiance[3].rgb;
  float3 L2m2 = g_probes[probe].irradiance[4].rgb;
  float3 L2m1 = g_probes[probe].irradiance[5].rgb;
  float3 L20 = g_probes[probe].irradiance[6].rgb;
  float3 L21 = g_probes[probe].irradiance[7].rgb;
  float3 L22 = g_probes[probe].irradiance[8].rgb;
  float3 result = c1 * L22 * (n.x * n.x - n.y * n.y) + c3 * L20 * n.z * n.z + c4 * L00 - c5 * L20 + 2.0 * c1 * (L2m2 * n.x * n.y + L21 * n.x * n.z + L2m1 * n.y * n.z) + 2.0 * c2 * (L11 * n.x + L1m1 * n.y + L10 * n.z);
  return max(result, 0.0);
}
// The same octahedral fold the shading pass reads a probe's visibility map through.
//
// Point sampled rather than filtered across the four texels around it, unlike in shading. There the
// filter is what keeps the visibility a surface is granted from changing in texel-sized blocks along
// it; here the texels are the subject, and blurring them would hide the very structure the view
// exists to show.
float2 UnpackDepth(uint packed) {
  return float2(f16tof32(packed & 0xFFFF), f16tof32(packed >> 16));
}
float2 SampleProbeDepth(uint probe, float3 direction) {
  float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
  float2 folded = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * float2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
  float2 uv = folded * 0.5 + 0.5;
  int2 texel = clamp(int2(uv * float(PROBE_DEPTH_RESOLUTION)), 0, PROBE_DEPTH_RESOLUTION - 1);
  return UnpackDepth(g_probes[probe].depth[texel.x + PROBE_DEPTH_RESOLUTION * texel.y]);
}
// How far the probe has walked from the lattice corner it was placed at, against how far it was let.
//
// The allowance is stored per probe rather than derived, because a probe cornering several leaves
// takes the smallest any of them grants, so there is no single figure to divide by from out here.
float ProbeTravelled(uint probe) {
  float allowance = g_probes[probe].anchor.w;
  if (allowance <= 0.0)
    return 0.0;
  return saturate(length(g_probes[probe].position.xyz - g_probes[probe].anchor.xyz) / allowance);
}
float3 ProbeColour(uint probe, float3 n) {
  float2 moments;
  float travelled;
  float trust = g_probes[probe].position.w;
  switch (u_mode) {
  case PROBE_VIEW_DISTANCE:
    moments = SampleProbeDepth(probe, n);
    return saturate(moments.x / max(u_range, 1e-6));
  case PROBE_VIEW_VARIANCE:
    moments = SampleProbeDepth(probe, n);
    return saturate(moments.y / max(u_range * PROBE_VARIANCE_SCALE, 1e-6));
  case PROBE_VIEW_TRUST:
    return saturate(trust);
  case PROBE_VIEW_RELOCATION:
    // Green at rest, red at the end of its allowance. A probe that has spent all of it and is still
    // where it does not want to be is a different complaint from one that never had to move, and the
    // two are a hue apart rather than a shade apart so that a row of them along a wall is legible.
    travelled = ProbeTravelled(probe);
    return lerp(float3(0.1, 0.8, 0.2), float3(0.9, 0.15, 0.1), travelled);
  case PROBE_VIEW_CLASSIFICATION:
    if (trust < PROBE_TRUSTED_LIMIT)
      return float3(0.9, 0.15, 0.1);
    return ProbeTravelled(probe) > PROBE_MOVED_LIMIT ? float3(0.15, 0.4, 0.9) : float3(0.1, 0.8, 0.2);
  default:
    return EvaluateProbe(probe, n) / PI;
  }
}
PSOutput PSMain(PSInput input) {
  PSOutput output;
  output.color = float4(ProbeColour(input.probe, normalize(input.normal)), 1.0);
  output.entityId = float4(1.0, 1.0, 1.0, 1.0);
  return output;
}
