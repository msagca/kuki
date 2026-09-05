static const float PI = 3.14159265359;
static const float EPSILON = 1.0e-6;
static const uint MAX_POINT_LIGHTS = KUKI_MAX_POINT_LIGHTS;
static const uint MAX_SPOT_LIGHTS = KUKI_MAX_SPOT_LIGHTS;
static const uint INVALID_INDEX = 0xFFFFFFFF;
static const uint RAYS_PER_PROBE = 64;
// Texels across one axis of a probe's visibility map, which the octahedron unfolds into a square of.
// Mirrors PROBE_DEPTH_RESOLUTION in dx_probe_volume.hpp, where the width is argued. There are more
// texels than rays, so a thread takes every RAYS_PER_PROBE'th one rather than exactly one.
static const int PROBE_DEPTH_RESOLUTION = 16;
static const uint PROBE_DEPTH_TEXELS = uint(PROBE_DEPTH_RESOLUTION * PROBE_DEPTH_RESOLUTION);
static const uint PROBE_BEHIND_WORDS = PROBE_DEPTH_TEXELS / 4;
// How tightly a ray must line up with a texel's direction to count towards it.
//
// This is the setting the test actually turns on, more than the map's width and far more than the
// ray count. It belongs to the texel, not to the rays: an exponent of T ln2 / 2 pi, where T is the
// texel count, puts the lobe's half-width at the angle one texel subtends, so a texel reports the
// surface in front of it rather than an average of its neighbourhood. Two hundred and fifty-six
// texels give twenty-eight.
//
// What the rays have to do is reach it. The worst-placed texel still has a ray within half the ray
// spacing, about thirteen degrees at sixty-four rays, and cos(13 degrees)^28 is around a half -- so
// every texel is fed, and sixty-four rays are enough. Sharpening further is what fails: the lobe
// would close inside that half-spacing and leave texels speaking for no ray at all.
// Now `u_params.w`. Tied to the map's width rather than free, so the derivation above is what any
// value put there has to answer to; the panel exposes it because the ray count it also depends on is
// the thing that cannot be exposed, and a scene that wants a different balance can only reach it
// from this side. See `IndirectLightingSettings::probeDepthSharpness`.
// How far a probe's visibility reaches, as a fraction of the volume's side. Mirrors
// PROBE_DEPTH_RANGE in dx_probe_volume.hpp, which seeds a fresh probe to it.
//
// Distances are clamped to this rather than kept, so a ray that escapes the scene cannot drag the
// mean out to the far plane and take the variance with it. The clamp needs room above the furthest
// the test is ever applied -- one leaf diagonal, about 1.73 times the coarsest leaf's spacing --
// because a texel averages a cone some twenty-five degrees wide and the distances feeding it can be
// twice what its centre direction measures. Sited too close, the grazing rays clamp while the
// central one does not, the mean lands under the true distance, and the probe invents an occluder in
// a ring at whatever radius the clamp begins to bite.
// Now `u_field.x`, and still seeded to the same value by `DXProbeVolume::Build`.
// The step off a surface and the weight floor are `u_probeTuning.x` and `.y`, out of the frame
// buffer rather than this one -- they are the shading pass's values, and this pass reading the field
// for second-bounce light has to read it the same way the shading pass will or the two disagree
// about where a point is. `scene.hlsl` is where they are described.
//
// The four relocation values are `u_relocation`, in the order they are declared there.
static const uint PROBE_NEIGHBOURS = 6;
static const uint VERTEX_STRIDE = 76;
static const uint VERTEX_NORMAL_OFFSET = 12;
static const uint VERTEX_TEXCOORD_OFFSET = 24;
static const uint ALBEDO_SLOT = 0;
static const uint EMISSIVE_SLOT = 6;
static const uint ALPHA_MODE_OPAQUE = 0;
static const uint ALPHA_MODE_MASK = 1;
// How much light must still be getting through before a ray stops being followed. Now `u_field.y`.
struct PointLight {
  float4 position;
  float4 diffuse;
  float4 specular;
  float4 attenuation;
};
struct SpotLight {
  float4 position;
  float4 direction;
  float4 diffuse;
  float4 specular;
  float4 attenuation;
  float4 cutoff;
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
struct OctreeNode {
  float3 center;
  float extent;
  uint4 children[2];
  uint4 probes[2];
  uint depth;
  uint leaf;
  uint2 padding;
};
struct Probe {
  float4 position;
  float4 anchor;
  float4 irradiance[9];
  uint depth[256];
  uint behind[64];
  uint neighbours[6];
  float exterior;
};
cbuffer TraceConstants : register(b0) {
  float4 u_rotation0;
  float4 u_rotation1;
  float4 u_rotation2;
  float4 u_volume;
  uint4 u_counts;
  float4 u_params;
  float4 u_relocation;
  float4 u_field;
};
cbuffer FrameConstants : register(b1) {
  float4x4 u_viewProjection;
  float4x4 u_lightViewProjection;
  float4x4 u_spotLightViewProjection[MAX_SPOT_LIGHTS];
  float4 u_viewPosition;
  float4 u_directionalDirection;
  float4 u_directionalAmbient;
  float4 u_directionalDiffuse;
  float4 u_directionalSpecular;
  float4 u_directionalIntensity;
  PointLight u_pointLights[MAX_POINT_LIGHTS];
  SpotLight u_spotLights[MAX_SPOT_LIGHTS];
  uint4 u_lightCounts;
  uint4 u_flags;
  float4 u_probeVolume;
  uint4 u_probeCounts;
  // Declared but unread, so that the two after it land where `DXFrameConstants` puts them. A cbuffer
  // packs by declaration order, and this pass skipping a vector the shading pass declares would
  // shift everything past it by sixteen bytes.
  uint4 u_debug;
  float4 u_indirectScale;
  float4 u_probeTuning;
};
RaytracingAccelerationStructure g_scene : register(t0);
StructuredBuffer<GeometryInfo> g_geometry : register(t1);
StructuredBuffer<OctreeNode> g_nodes : register(t2);
StructuredBuffer<uint> g_lookup : register(t3);
/// The sky's radiance as nine spherical harmonic coefficients, the same ones the irradiance map is
/// convolved from. Read only when the frame says a skybox has been prepared.
StructuredBuffer<float4> g_skyHarmonics : register(t4);
RWStructuredBuffer<Probe> g_probes : register(u0);
ByteAddressBuffer g_meshBuffers[] : register(t0, space1);
Texture2D g_materialTextures[] : register(t0, space2);
SamplerState g_sampler : register(s0);
groupshared float3 gs_radiance[RAYS_PER_PROBE];
groupshared float3 gs_direction[RAYS_PER_PROBE];
groupshared float gs_distance[RAYS_PER_PROBE];
/// @brief One per ray that came back off the far side of a surface, nought for the rest.
///
/// Kept per ray rather than reduced to the nearest of them, because relocation needs a direction and
/// the nearest single ray is the worst estimate of one available. See the escape below.
groupshared float gs_behind[RAYS_PER_PROBE];
/// @brief The finished backface share of each depth texel, on its way to being packed.
///
/// Staged rather than written where it is worked out. The map is walked strided, four texels to a
/// thread, so the four texels of any one packed word belong to four different threads and no
/// thread may write a byte of it without racing the others for the read and write around it.
groupshared float gs_behindTexel[PROBE_DEPTH_TEXELS];
groupshared uint gs_buried;
groupshared float gs_trust;
float3 SphericalFibonacci(uint index, uint count) {
  float phi = 2.0 * PI * frac(float(index) * 0.6180339887498949);
  float cosTheta = 1.0 - (2.0 * float(index) + 1.0) / float(count);
  float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
  return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}
float3 Rotate(float3 direction) {
  return float3(dot(u_rotation0.xyz, direction), dot(u_rotation1.xyz, direction), dot(u_rotation2.xyz, direction));
}
void SHBasis(float3 d, out float basis[9]) {
  basis[0] = 0.282095;
  basis[1] = 0.488603 * d.y;
  basis[2] = 0.488603 * d.z;
  basis[3] = 0.488603 * d.x;
  basis[4] = 1.092548 * d.x * d.y;
  basis[5] = 1.092548 * d.y * d.z;
  basis[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);
  basis[7] = 1.092548 * d.x * d.z;
  basis[8] = 0.546274 * (d.x * d.x - d.y * d.y);
}
float3 EvaluateIrradiance(uint probe, float3 n) {
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
// Where a direction lands on a probe's visibility map, in map coordinates from nought to one.
//
// The map is an octahedron unfolded into a square: the z-positive hemisphere fills the middle
// diamond and the other folds out into the four corners. It is the cheapest sphere-to-square mapping
// with no pole and no cluster, which is what this needs -- a probe is read in whatever direction a
// shading point happens to lie in, and no direction deserves more texels than another.
float2 OctahedralEncode(float3 direction) {
  float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
  float2 folded = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * float2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
  return folded * 0.5 + 0.5;
}
// The direction a point on the map stands for, which is the encoding run backwards.
float3 OctahedralDecode(float2 uv) {
  float2 f = uv * 2.0 - 1.0;
  float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
  float fold = saturate(-n.z);
  n.xy += float2(n.x >= 0.0 ? -fold : fold, n.y >= 0.0 ? -fold : fold);
  return normalize(n);
}
// Brings a texel that stepped off the edge of the map back to the one it is really beside.
//
// The unfolded square's boundary is a single closed seam: leaving through an edge re-enters through
// that same edge mirrored about its midpoint. Wrapping rather than clamping is what keeps a bilinear
// read continuous across the fold, and a corner leaves through two edges at once, which is why the
// two mirrors compose rather than one of them winning.
int2 OctahedralWrap(int2 texel) {
  if (texel.x < 0 || texel.x >= PROBE_DEPTH_RESOLUTION) {
    texel.x = texel.x < 0 ? 0 : PROBE_DEPTH_RESOLUTION - 1;
    texel.y = PROBE_DEPTH_RESOLUTION - 1 - texel.y;
  }
  if (texel.y < 0 || texel.y >= PROBE_DEPTH_RESOLUTION) {
    texel.y = texel.y < 0 ? 0 : PROBE_DEPTH_RESOLUTION - 1;
    texel.x = PROBE_DEPTH_RESOLUTION - 1 - texel.x;
  }
  return texel;
}
// The mean distance a probe recorded looking in a direction and the variance of that distance,
// filtered across the four texels around it so the visibility a surface is granted varies smoothly
// along it rather than in blocks the size of a texel.
//
// A texel stores a mean and a standard deviation, and four of them are a mixture of four
// distributions rather than four samples of one. A mixture's spread is not the average of the
// spreads: two texels a metre apart in mean are spread across that metre however narrow each of them
// is on its own. So the second moment is what gets averaged, and the variance is formed from it at
// the end -- the law of total variance, which the averaged deviation this used to return is missing
// the between-texel half of.
//
// That half is the whole of the problem at a silhouette, which is where the test is load bearing.
// One texel holds a near surface with almost no spread, its neighbour holds open space with almost
// no spread; averaging the deviations put the mean between the two and left the spread near nought,
// so the probe asserted a surface at a distance nothing was at and asserted it with total
// confidence. Every point past that invented surface was refused, and refused abruptly, which drew
// a hard edge in the bounce along every silhouette in the map. Composed properly the same pair reads
// as wide, the Chebyshev bound goes slack, and the test softens where it is least sure -- which is
// the direction the uncertainty actually runs.
float2 UnpackDepth(uint packed) {
  return float2(f16tof32(packed & 0xFFFF), f16tof32(packed >> 16));
}
float UnpackBehind(uint word, uint slot) {
  return float((word >> (slot * 8)) & 0xFF) / 255.0;
}
// The mean and variance as before, and with them the share of the record that was written from
// behind. Filtered across the same four texels on the same weights: a verdict stored per texel
// comes back as a fraction between them, so the gate it feeds moves smoothly across a surface
// rather than in steps the size of a texel, and no threshold is needed anywhere to make it so.
float3 SampleProbeDepth(uint probe, float3 direction) {
  float2 uv = OctahedralEncode(direction) * float(PROBE_DEPTH_RESOLUTION) - 0.5;
  int2 base = int2(floor(uv));
  float2 fraction = uv - float2(base);
  float mean = 0.0;
  float second = 0.0;
  float behind = 0.0;
  for (int corner = 0; corner < 4; ++corner) {
    int2 delta = int2(corner & 1, corner >> 1);
    int2 texel = OctahedralWrap(base + delta);
    float2 blend = float2(delta.x != 0 ? fraction.x : 1.0 - fraction.x, delta.y != 0 ? fraction.y : 1.0 - fraction.y);
    uint index = uint(texel.x + PROBE_DEPTH_RESOLUTION * texel.y);
    float2 pair = UnpackDepth(g_probes[probe].depth[index]);
    float share = blend.x * blend.y;
    mean += pair.x * share;
    // the texel's own spread and where its mean sits, which is what carries the between-texel half
    second += (pair.y * pair.y + pair.x * pair.x) * share;
    behind += UnpackBehind(g_probes[probe].behind[index >> 2], index & 3) * share;
  }
  return float3(mean, max(second - mean * mean, 0.0), behind);
}
// How much of a probe's estimate a point is entitled to, given what the probe can see of it.
//
// Interpolation is what makes a probe field smooth and is also the whole of its leakage: the eight
// corners of a cell are blended by distance alone, and distance cannot tell that a wall stands
// between one of them and the point being shaded. The probe already knows -- its rays measured how
// far the scene is in every direction -- so the fix is to ask it.
//
// A single mean distance would answer yes or no and alias along every silhouette, where one texel
// covers a near surface and a far one at once. Two moments give a variance instead, and Chebyshev's
// inequality turns a mean and a variance into a bound on how likely a sample is to exceed a value.
// It is the sharpest such bound that assumes nothing about the distribution, which suits one
// assembled out of sixty-four rays and no knowledge of the geometry. Where a texel saw one flat
// surface the variance is near nought and the answer is nearly binary; where it straddled an edge
// the variance is large and the answer softens, which is the direction the uncertainty runs. Cubing
// sharpens what is otherwise a loose bound, the inequality being an upper limit and generous in the
// middle of its range.
float ProbeVisibility(uint probe, float3 probePosition, float3 position) {
  float3 offset = position - probePosition;
  float separation = length(offset);
  if (separation < EPSILON)
    return 1.0;
  float3 moments = SampleProbeDepth(probe, offset / separation);
  if (separation <= moments.x)
    return 1.0;
  // Already a variance. It is squared where it is stored, per texel, and composed as one across the
  // four -- squaring the composed figure again here would undo that and restore the hard edge.
  float variance = moments.y;
  float excess = separation - moments.x;
  float chebyshev = variance / (variance + excess * excess);
  // Refused outright where the probe measured this direction from the far side of something. The
  // Chebyshev bound above is a bound and nothing more: given an excess of a fraction of a probe
  // spacing -- which is all a wall of no thickness ever offers it -- it answers near one, and a
  // probe believed at near one through a wall is that wall lit from the wrong side. Orientation
  // answers the same question exactly and needs no thickness to do it, so it multiplies the bound
  // rather than being folded into the distance it is blind to.
  return chebyshev * chebyshev * chebyshev * (1.0 - moments.z);
}
bool FindLeaf(float3 position, out OctreeNode node) {
  node = (OctreeNode)0;
  float3 local = (position - u_volume.xyz) / u_volume.w;
  if (any(local < 0.0) || any(local >= 1.0))
    return false;
  uint resolution = u_counts.w;
  uint3 cell = min(uint3(local * float(resolution)), resolution - 1);
  uint leafIndex = g_lookup[cell.x + resolution * (cell.y + resolution * cell.z)];
  if (leafIndex >= u_counts.y)
    return false;
  node = g_nodes[leafIndex];
  return node.leaf != 0;
}
float3 SampleVolume(float3 position, float3 normal) {
  OctreeNode node;
  if (!FindLeaf(position, node))
    return 0.0;
  // Sampled a step off the surface rather than on it. A point lying exactly on a wall is the same
  // distance from a probe as the wall is, so the visibility test cannot separate the two and reads
  // the surface as its own occluder. The normal is the direction that breaks that tie, and the step
  // is a fraction of the local leaf's spacing so it stays proportionate wherever the octree refined.
  float3 biased = position + normal * (u_probeTuning.x * 2.0 * node.extent);
  OctreeNode biasedNode;
  if (FindLeaf(biased, biasedNode))
    node = biasedNode;
  else
    biased = position;
  float3 t = saturate((biased - (node.center - node.extent)) / (2.0 * node.extent));
  // Taken straight. This used to be eased -- t*t*(3-2t) -- to make the slope meet itself where one
  // cell hands over to the next, on the reasoning that trilinear interpolation is continuous in
  // value but not in gradient and the eye finds a break in a gradient more readily than a break in a
  // level. The reasoning is sound and the cure was worse than the complaint.
  //
  // Easing does not interpolate the field, it interpolates a curve through it. Across a cell where
  // the light varies smoothly -- which is most of a room -- the true value runs very nearly straight
  // between the two probes, and straight is exactly what plain trilinear returns. Easing bends that
  // line into an S: flat where it meets each face, steep through the middle, and away from the truth
  // by up to 0.0962 of the step between the probes, every cell, in the same place in every cell.
  // What the eye is offered is not a smoother gradient but a periodic one, ruled at the cell
  // spacing, and that is the grid.
  //
  // The orders are what settle it. Plain trilinear is wrong by something in the square of the
  // spacing and reproduces a straight ramp exactly; the easing is wrong by something in the spacing
  // itself. So the easing dominates as probes get closer, which is why halving the spacing only
  // halved the pattern instead of quartering it, and why it survived every improvement to the field
  // it was drawn on. A slope that meets itself is worth having, but not at the price of first order
  // error, and not when the field it is smoothing was already smooth.
  float3 total = 0.0;
  float weightSum = 0.0;
  float3 plainTotal = 0.0;
  float plainWeight = 0.0;
  for (uint corner = 0; corner < 8; ++corner) {
    uint index = node.probes[corner >> 2][corner & 3];
    if (index >= u_counts.x)
      continue;
    float3 axis = float3((corner & 1) ? t.x : 1.0 - t.x, (corner & 2) ? t.y : 1.0 - t.y, (corner & 4) ? t.z : 1.0 - t.z);
    float share = axis.x * axis.y * axis.z;
    if (share <= 0.0)
      continue;
    float3 probePosition = g_probes[index].position.xyz;
    float3 offset = probePosition - biased;
    float length2 = dot(offset, offset);
    float weight = 1.0;
    if (length2 > EPSILON) {
      // Wrapped rather than clamped. A probe level with the surface keeps a quarter of its say
      // and only one directly behind it loses all of it; clamping at the horizon instead handed
      // almost the whole answer to the one or two corners nearest the normal, and a cell answered
      // by one corner is a cell its neighbours answer differently -- which is the lattice, printed
      // onto the floor. The floor underneath is what the crush below measures itself against, so it
      // stays where it was.
      float facing = dot(offset * rsqrt(length2), normal) * 0.5 + 0.5;
      weight = facing * facing + 0.05;
    }
    float3 irradiance = EvaluateIrradiance(index, normal);
    plainTotal += irradiance * weight * share;
    plainWeight += weight * share;
    weight *= ProbeVisibility(index, probePosition, biased);
    // Crushed before the trilinear share is applied rather than after. The crush is there to send a
    // probe the point can barely see the rest of the way to nothing; a probe that is merely far from
    // the point already has a small share and needs no help. Cubing the product instead drives the
    // ordinary far corners of every cell to nothing as well, which leaves the nearest probe alone in
    // deciding the answer and prints the lattice onto the wall.
    if (weight < u_probeTuning.y)
      weight *= weight * weight / max(u_probeTuning.y * u_probeTuning.y, EPSILON);
    weight *= share;
    total += irradiance * weight;
    weightSum += weight;
  }
  if (weightSum > EPSILON)
    return total / weightSum;
  // Nothing could see this point -- it is inside geometry, or in a pocket none of the eight reach.
  // Interpolating anyway beats returning black, black being the failure this test exists to prevent
  // and a perverse thing to introduce while preventing it.
  return plainWeight > 0.0 ? plainTotal / plainWeight : 0.0;
}
uint3 FetchIndices(GeometryInfo info, uint primitive) {
  if (info.indexBuffer == INVALID_INDEX)
    return uint3(primitive * 3, primitive * 3 + 1, primitive * 3 + 2);
  return g_meshBuffers[NonUniformResourceIndex(info.indexBuffer)].Load3(primitive * 12);
}
float3 FetchNormal(GeometryInfo info, uint vertex) {
  return asfloat(g_meshBuffers[NonUniformResourceIndex(info.vertexBuffer)].Load3(vertex * VERTEX_STRIDE + VERTEX_NORMAL_OFFSET));
}
float2 FetchTexcoord(GeometryInfo info, uint vertex) {
  return asfloat(g_meshBuffers[NonUniformResourceIndex(info.vertexBuffer)].Load2(vertex * VERTEX_STRIDE + VERTEX_TEXCOORD_OFFSET));
}
float4 FetchAlbedo(GeometryInfo info, float2 texcoord) {
  float4 albedo = info.albedo;
  if (info.materialTextures != INVALID_INDEX && (info.textureMask & (1u << ALBEDO_SLOT)))
    albedo *= g_materialTextures[NonUniformResourceIndex(info.materialTextures + ALBEDO_SLOT)].SampleLevel(g_sampler, texcoord, 0);
  return albedo;
}
float3 VolumeAttenuation(GeometryInfo info) {
  if (info.attenuation.w <= 0.0 || info.thickness <= 0.0)
    return 1.0;
  float3 coefficient = -log(max(info.attenuation.rgb, EPSILON)) / info.attenuation.w;
  return exp(-coefficient * info.thickness);
}
float3 SurfaceTransmittance(GeometryInfo info, float2 texcoord) {
  float4 albedo = FetchAlbedo(info, texcoord);
  if (info.alphaMode == ALPHA_MODE_MASK)
    return albedo.a < info.alphaCutoff ? 1.0 : 0.0;
  float coverage = info.alphaMode == ALPHA_MODE_OPAQUE ? 1.0 : saturate(albedo.a);
  float3 through = info.transmission > 0.0 ? saturate(info.transmission) * albedo.rgb * VolumeAttenuation(info) : 0.0;
  return saturate(1.0 - coverage + coverage * through);
}
float Loudest(float3 value) {
  return max(value.r, max(value.g, value.b));
}
float2 CandidateTexcoord(GeometryInfo info, uint primitive, float2 barycentrics) {
  uint3 indices = FetchIndices(info, primitive);
  float3 weights = float3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
  return FetchTexcoord(info, indices.x) * weights.x + FetchTexcoord(info, indices.y) * weights.y + FetchTexcoord(info, indices.z) * weights.z;
}
float3 Transmittance(float3 origin, float3 direction, float maximum) {
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = 0.001;
  ray.TMax = maximum;
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
  query.TraceRayInline(g_scene, RAY_FLAG_NONE, 0xFF, ray);
  float3 transmittance = 1.0;
  while (query.Proceed()) {
    if (query.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE)
      continue;
    GeometryInfo info = g_geometry[query.CandidateInstanceID()];
    float3 through = SurfaceTransmittance(info, CandidateTexcoord(info, query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()));
    if (Loudest(through) < u_field.y)
      query.CommitNonOpaqueTriangleHit();
    else
      transmittance *= through;
  }
  if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    return 0.0;
  return transmittance;
}
// The irradiance the sky puts on a surface facing this way.
//
// Second order harmonics carry a sky as a soft gradient and nothing sharper, which is all a bounce
// needs: what leaves this surface is spread over its whole hemisphere and cannot hold the detail a
// sharper sky would have had anyway.
float3 SkyIrradiance(float3 normal) {
  const float c1 = 0.429043;
  const float c2 = 0.511664;
  const float c3 = 0.743125;
  const float c4 = 0.886227;
  const float c5 = 0.247708;
  float3 L00 = g_skyHarmonics[0].rgb;
  float3 L1m1 = g_skyHarmonics[1].rgb;
  float3 L10 = g_skyHarmonics[2].rgb;
  float3 L11 = g_skyHarmonics[3].rgb;
  float3 L2m2 = g_skyHarmonics[4].rgb;
  float3 L2m1 = g_skyHarmonics[5].rgb;
  float3 L20 = g_skyHarmonics[6].rgb;
  float3 L21 = g_skyHarmonics[7].rgb;
  float3 L22 = g_skyHarmonics[8].rgb;
  float3 n = normal;
  float3 result = c1 * L22 * (n.x * n.x - n.y * n.y) + c3 * L20 * n.z * n.z + c4 * L00 - c5 * L20 + 2.0 * c1 * (L2m2 * n.x * n.y + L21 * n.x * n.z + L2m1 * n.y * n.z) + 2.0 * c2 * (L11 * n.x + L1m1 * n.y + L10 * n.z);
  return max(result, 0.0);
}
// What the sky contributes to a surface a probe ray landed on.
//
// Without this the sky lights what the camera can see and nothing else: a wall lit by the sky throws
// no light of its own, because the trace only ever knew about the lamps. Indoors it changes nothing,
// which is why it went unnoticed here; outdoors, where the sky is most of the light there is, it is
// most of the bounce that was missing.
//
// Whether the sky reaches this point is asked with a single ray along the normal rather than over
// the hemisphere. One sample is a coarse answer to "how much sky is above me", but it separates the
// two cases that matter -- open ground, where it escapes, and a sealed room, where it does not --
// and this is light that has already bounced once, where a coarse answer costs little.
float3 SkyLight(float3 position, float3 normal) {
  if (u_flags.x == 0 || u_flags.z == 0)
    return 0.0;
  float3 open = Transmittance(position, normal, u_params.y);
  return open * SkyIrradiance(normal);
}
float3 DirectLight(float3 position, float3 normal) {
  float3 total = 0.0;
  if (u_lightCounts.z != 0) {
    float3 L = normalize(-u_directionalDirection.xyz);
    float cosine = saturate(dot(normal, L));
    if (cosine > 0.0)
      total += u_directionalDiffuse.rgb * u_directionalIntensity.x * cosine * Transmittance(position, L, u_params.y);
  }
  for (uint p = 0; p < min(u_lightCounts.x, MAX_POINT_LIGHTS); ++p) {
    float3 offset = u_pointLights[p].position.xyz - position;
    float distance = length(offset);
    float3 L = offset / max(distance, EPSILON);
    float cosine = saturate(dot(normal, L));
    if (cosine <= 0.0)
      continue;
    float3 a = u_pointLights[p].attenuation.xyz;
    float attenuation = u_pointLights[p].attenuation.w / (a.x + a.y * distance + a.z * distance * distance);
    total += u_pointLights[p].diffuse.rgb * attenuation * cosine * Transmittance(position, L, distance - 0.01);
  }
  for (uint s = 0; s < min(u_lightCounts.y, MAX_SPOT_LIGHTS); ++s) {
    float3 offset = u_spotLights[s].position.xyz - position;
    float distance = length(offset);
    float3 L = offset / max(distance, EPSILON);
    float cosine = saturate(dot(normal, L));
    if (cosine <= 0.0)
      continue;
    float theta = dot(L, normalize(-u_spotLights[s].direction.xyz));
    float cone = saturate((theta - u_spotLights[s].cutoff.y) / max(u_spotLights[s].cutoff.x - u_spotLights[s].cutoff.y, EPSILON));
    if (cone <= 0.0)
      continue;
    float3 a = u_spotLights[s].attenuation.xyz;
    float attenuation = u_spotLights[s].attenuation.w * cone / (a.x + a.y * distance + a.z * distance * distance);
    total += u_spotLights[s].diffuse.rgb * attenuation * cosine * Transmittance(position, L, distance - 0.01);
  }
  return total;
}
float3 ShadeHit(GeometryInfo info, float3 position, float3 normal, float2 texcoord) {
  float3 albedo = FetchAlbedo(info, texcoord).rgb;
  float3 emissive = info.emissive.rgb;
  if (info.materialTextures != INVALID_INDEX && (info.textureMask & (1u << EMISSIVE_SLOT)))
    emissive *= g_materialTextures[NonUniformResourceIndex(info.materialTextures + EMISSIVE_SLOT)].SampleLevel(g_sampler, texcoord, 0).rgb;
  float3 incoming = DirectLight(position, normal) + SampleVolume(position, normal) + SkyLight(position, normal);
  return emissive + albedo * incoming / PI;
}
[numthreads(RAYS_PER_PROBE, 1, 1)]
void CSTrace(uint3 group : SV_GroupID, uint thread : SV_GroupIndex) {
  uint probe = group.x;
  if (probe >= u_counts.x)
    return;
  if (thread == 0)
    gs_buried = 0;
  GroupMemoryBarrierWithGroupSync();
  float3 origin = g_probes[probe].position.xyz;
  float3 direction = Rotate(SphericalFibonacci(thread, RAYS_PER_PROBE));
  RayDesc ray;
  ray.Origin = origin;
  ray.Direction = direction;
  ray.TMin = 0.0;
  ray.TMax = u_params.y;
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
  query.TraceRayInline(g_scene, RAY_FLAG_NONE, 0xFF, ray);
  float3 throughput = 1.0;
  while (query.Proceed()) {
    if (query.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE)
      continue;
    GeometryInfo candidate = g_geometry[query.CandidateInstanceID()];
    float3 through = SurfaceTransmittance(candidate, CandidateTexcoord(candidate, query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()));
    if (Loudest(through) < u_field.y)
      query.CommitNonOpaqueTriangleHit();
    else
      throughput *= through;
  }
  float3 radiance = 0.0;
  float behind = 0.0;
  // A ray that hit nothing is recorded at the full range rather than dropped, since "nothing out
  // this way" is exactly what a point needs to know to trust the probe in that direction.
  float range = u_field.x * u_volume.w;
  float travelled = range;
  if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    travelled = min(query.CommittedRayT(), range);
    GeometryInfo info = g_geometry[query.CommittedInstanceID()];
    uint3 indices = FetchIndices(info, query.CommittedPrimitiveIndex());
    float2 bary = query.CommittedTriangleBarycentrics();
    float3 weights = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
    float3 objectNormal = FetchNormal(info, indices.x) * weights.x + FetchNormal(info, indices.y) * weights.y + FetchNormal(info, indices.z) * weights.z;
    float2 texcoord = FetchTexcoord(info, indices.x) * weights.x + FetchTexcoord(info, indices.y) * weights.y + FetchTexcoord(info, indices.z) * weights.z;
    float3x4 worldToObject = query.CommittedWorldToObject3x4();
    float3 normal = normalize(mul(objectNormal, (float3x3)worldToObject));
    // Meeting a surface from behind says nothing about the light and everything about where this
    // probe is standing: it is on the unlit side of something. Noted before the normal is turned to
    // face the ray, which is the last moment the distinction survives.
    if (dot(normal, direction) > 0.0) {
      normal = -normal;
      behind = 1.0;
      InterlockedAdd(gs_buried, 1);
    }
    float3 position = origin + direction * query.CommittedRayT() + normal * u_params.z;
    radiance = throughput * ShadeHit(info, position, normal, texcoord);
  }
  gs_radiance[thread] = radiance;
  gs_direction[thread] = direction;
  gs_distance[thread] = travelled;
  gs_behind[thread] = behind;
  GroupMemoryBarrierWithGroupSync();
  // The visibility map, each texel folding in every ray by how well it lines up. There are four
  // texels per thread, taken strided so the group walks the map together rather than in blocks.
  for (uint texel = thread; texel < PROBE_DEPTH_TEXELS; texel += RAYS_PER_PROBE) {
    float3 texelDirection = OctahedralDecode((float2(int(texel) % PROBE_DEPTH_RESOLUTION, int(texel) / PROBE_DEPTH_RESOLUTION) + 0.5) / float(PROBE_DEPTH_RESOLUTION));
    float2 moments = 0.0;
    float behind = 0.0;
    float share = 0.0;
    for (uint d = 0; d < RAYS_PER_PROBE; ++d) {
      float cosine = dot(texelDirection, gs_direction[d]);
      if (cosine <= 0.0)
        continue;
      float lobe = pow(cosine, u_params.w);
      moments += float2(gs_distance[d], gs_distance[d] * gs_distance[d]) * lobe;
      behind += gs_behind[d] * lobe;
      share += lobe;
    }
    behind = share > EPSILON ? behind / share : 0.0;
    moments = share > EPSILON ? moments / share : float2(range, range * range);
    // the spread is formed here, in full precision, rather than left for reconstruction to recover
    // by subtracting one stored number from another
    float2 pair = float2(moments.x, sqrt(max(0.0, moments.y - moments.x * moments.x)));
    // blended on the same schedule as the irradiance, so the geometry a probe believes in and the
    // light it believes in are never a different number of frames old
    pair = lerp(pair, UnpackDepth(g_probes[probe].depth[texel]), u_params.x);
    g_probes[probe].depth[texel] = f32tof16(pair.x) | (f32tof16(pair.y) << 16);
    // on the same schedule, so which side a probe is of a surface converges with how far away it
    // thinks the surface is, rather than switching about under a ray set that rotates every frame
    gs_behindTexel[texel] = lerp(behind, UnpackBehind(g_probes[probe].behind[texel >> 2], texel & 3), u_params.x);
  }
  GroupMemoryBarrierWithGroupSync();
  for (uint word = thread; word < PROBE_BEHIND_WORDS; word += RAYS_PER_PROBE) {
    uint packed = 0;
    for (uint slot = 0; slot < 4; ++slot)
      packed |= uint(saturate(gs_behindTexel[word * 4 + slot]) * 255.0 + 0.5) << (slot * 8);
    g_probes[probe].behind[word] = packed;
  }
  if (thread == 0) {
    float4 anchor = g_probes[probe].anchor;
    float4 placement = g_probes[probe].position;
    // Where the room is, averaged over every ray that came back off a far side rather than taken
    // from the nearest one of them.
    //
    // The nearest is the obvious choice and it is the one that flickers. The ray set is turned by a
    // fresh random rotation every trace, so "which ray was nearest" is redrawn from scratch each
    // frame: for a probe sitting just above a ceiling, every downward ray hits it, the shortest is
    // whichever happens to be most vertical this frame, and its direction wanders by the ray spacing
    // -- thirteen degrees -- from one frame to the next. Three quarters of that step is taken, so
    // the probe chases a target that never settles, and a probe that never settles is a probe whose
    // estimate is of somewhere slightly different every frame. That is the flicker.
    //
    // A mean over all of them is the same direction each frame whatever the rotation, because it is
    // a property of the geometry rather than of the sample: above a floor it points down, below a
    // ceiling it points up, and where a probe is caught between two surfaces facing the same way the
    // opposing halves cancel and it asks to stay where it is -- which is the honest answer for a
    // probe with nowhere to go, and leaves `trust` to hand it to its neighbours rather than leaving
    // it thrashing between two walls it cannot clear.
    float3 escape = 0.0;
    float facing = 0.0;
    for (uint back = 0; back < RAYS_PER_PROBE; ++back) {
      // Weighted by how near the surface is, and near is measured against the allowance, because
      // the allowance is the whole of what this can do about it. A backface further off than the
      // probe can move is not something the probe is buried in; it is scenery, and letting it steer
      // is what put these probes where they were.
      //
      // Unweighted, a probe just above a ceiling counted every downward ray alike, including the
      // grazing ones landing two metres out. Near the edge of the ceiling those rays run out on one
      // side and not the other, so their mean tilted towards the middle of the room, and the probe
      // spent its allowance travelling sideways instead of the four centimetres downward that would
      // have cleared the surface. It ended up pinned on the allowance sphere, still inside the
      // ceiling, sliding around it as the tilt wandered -- which is a probe that never stops moving.
      float near = gs_behind[back] * saturate(1.0 - gs_distance[back] / max(anchor.w, EPSILON));
      escape += gs_direction[back] * near;
      facing += near;
    }
    float3 wanted = placement.xyz;
    if (facing > 0.0) {
      // How much those rays agree, which is what decides whether they are pointing anywhere at all.
      //
      // A probe just outside a surface sees a clean hemisphere of it and the mean of those rays is
      // half a unit long. A probe sealed inside solid geometry sees the inside of it in every
      // direction at once, and the mean of all sixty-four is 0.0016 -- not a direction, just the
      // residue of a ray set that does not sum to exactly nothing. Normalising that residue is how a
      // probe with nowhere to go acquires a full length step, and because the set is turned by a
      // fresh rotation every trace the step points somewhere new each frame: a probe that never
      // settles, and an estimate taken from somewhere slightly different every frame. The two cases
      // are three hundred times apart, so anything between them separates them; this sits nearer the
      // quiet end, since a probe that could have escaped and did not is a probe its neighbours cover
      // for, and one that moves when it should not is a flicker in the picture.
      float3 mean = escape / facing;
      float agreement = length(mean);
      if (agreement > u_relocation.z) {
        // Stopped where the step leaves the allowance, rather than stepped past it and pulled back.
        //
        // Pulling back is what the clamp below does, and for a probe that has spent its allowance
        // and is still buried it turns a push the probe cannot follow into a slide it can: the step
        // goes outwards, the clamp returns it to the sphere, and what survives of it is the part
        // along the surface. The probe then circles its own allowance for as long as the scene
        // stands still -- moving constantly, for a reason that has nothing to do with where it ought
        // to be. Solving for where the step crosses the sphere lets it travel exactly as far as it
        // is allowed and not one step further: at the boundary and still pushing outwards it is
        // allowed nothing, and holds. A push that points back inwards is untouched by this, so a
        // probe that the scene changing has freed is still free to take it.
        float3 heading = mean / agreement;
        float3 from = placement.xyz - anchor.xyz;
        float along = dot(from, heading);
        float outside = dot(from, from) - anchor.w * anchor.w;
        float travel = min(u_relocation.x * anchor.w, -along + sqrt(max(along * along - outside, 0.0)));
        wanted = placement.xyz + heading * max(travel, 0.0);
      }
    }
    // Measured from the anchor every trace rather than accumulated, so the allowance bounds where a
    // probe can be and not merely how fast it drifts there. With nothing behind it, a probe asks for
    // where it already is and holds still.
    float3 offset = wanted - anchor.xyz;
    float reach = dot(offset, offset);
    if (reach > anchor.w * anchor.w)
      offset *= anchor.w * rsqrt(reach);
    // How much of its own estimate this probe is trusted with. Settled on the same schedule as
    // everything else, so a probe near the line lands on one answer rather than crossing it back and
    // forth as the rotation changes which rays meet what.
    float sealed = float(gs_buried) / float(RAYS_PER_PROBE);
    // A probe the build found standing behind the scene's surfaces is never trusted with its own
    // estimate, however open its view. An open view of the void outside a wall is precisely the case
    // its own rays cannot report, because there is nothing out there for them to come back off: the
    // probe looks as healthy as one in the middle of the room and holds the sky instead of the room.
    // It takes its neighbours' estimate, which is what leaves the trilinear blend on that wall's
    // inner face with nothing in it that repeats with the lattice.
    float own = saturate(1.0 - sealed / max(u_relocation.w, EPSILON)) * (1.0 - g_probes[probe].exterior);
    float trust = lerp(own, placement.w, u_params.x);
    gs_trust = trust;
    g_probes[probe].position = float4(lerp(anchor.xyz + offset, placement.xyz, u_relocation.y), trust);
  }
  // every thread waits, because a thread that has returned cannot arrive at a barrier
  GroupMemoryBarrierWithGroupSync();
  float trusted = gs_trust;
  if (thread >= 9)
    return;
  float3 sum = 0.0;
  for (uint r = 0; r < RAYS_PER_PROBE; ++r) {
    float basis[9];
    SHBasis(gs_direction[r], basis);
    sum += gs_radiance[r] * basis[thread];
  }
  sum *= 4.0 * PI / float(RAYS_PER_PROBE);
  float3 previous = g_probes[probe].irradiance[thread].rgb;
  // a running mean while the scene holds still, so the field converges instead of circling; the
  // weight is the processor's to decide, since only it knows how many estimates are already here
  float3 settled = lerp(sum, previous, u_params.x);
  if (trusted < 1.0) {
    // Sealed in geometry, so its own rays have nothing to say about this room. Borrowing from the
    // neighbours that do leaves it holding a plausible value instead of black, which is what lets
    // shading interpolate every probe evenly and stop drawing lines where it would have rejected one.
    //
    // Each neighbour is counted by how far it is believed, so a probe filled in this way is never
    // itself a source. Neighbours are read as they stand, which within a dispatch may be this frame's
    // estimate or last frame's; against a mean that takes tens of frames to settle, the difference
    // does not survive to be seen.
    float3 borrowed = 0.0;
    float belief = 0.0;
    float3 relayed = 0.0;
    float relays = 0.0;
    for (uint n = 0; n < PROBE_NEIGHBOURS; ++n) {
      uint neighbour = g_probes[probe].neighbours[n];
      if (neighbour >= u_counts.x)
        continue;
      float weight = g_probes[neighbour].position.w;
      borrowed += g_probes[neighbour].irradiance[thread].rgb * weight;
      belief += weight;
      relayed += g_probes[neighbour].irradiance[thread].rgb;
      relays += 1.0;
    }
    if (belief > EPSILON) {
      settled = lerp(borrowed / belief, settled, trusted);
    } else if (relays > 0.0) {
      // Nothing next to this probe is trusted either, so there is no estimate of the room within one
      // step to borrow. Averaging with the neighbours instead passes whatever they borrowed one probe
      // further in, and repeating it every trace walks a value out through a region of distrusted
      // probes however thick that region is -- a probe per frame, settling in as many frames as it is
      // probes across, which for the shell around a scene is two or three.
      //
      // The believed mean above cannot do this. Every probe in the middle of such a region has belief
      // of nought, so it would keep its own estimate, and for a probe outside a wall its own estimate
      // is the sky. That rule was right while the only distrusted probes were the ones sealed inside
      // geometry, which are isolated -- a probe inside a wall has believed neighbours on either side
      // of it. A region several probes across is the case it did not cover.
      //
      // The probe counts itself, which is what makes this safe rather than merely useful. Without it
      // the step is a mean of the neighbours alone, and on the first trace every neighbour is still
      // nought: the probe writes black, so does every probe in the region, and a region holding no
      // trusted probe anywhere then averages black with black for as long as the scene stands. The
      // field never recovers, because a distrusted probe's own estimate is not in the sum that could
      // bring it back. Counting itself puts that estimate back in, and costs nothing where the region
      // does have a trusted probe to walk from: a mean that includes the probe itself settles on the
      // same value as a mean that does not -- five times this probe equals this probe plus the four
      // neighbours, so this probe equals their mean either way -- and only takes a few more traces
      // to get there.
      settled = (settled + relayed) / (relays + 1.0);
    }
  }
  g_probes[probe].irradiance[thread] = float4(settled, 0.0);
}
