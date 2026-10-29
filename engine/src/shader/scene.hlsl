static const uint ALPHA_MODE_OPAQUE = 0;
static const uint ALPHA_MODE_MASK = 1;
static const float EPSILON = 1.0e-6;
static const float PI = 3.14159265359;
static const float MAX_REFLECTION_LOD = 6.0;
static const uint MAX_POINT_LIGHTS = KUKI_MAX_POINT_LIGHTS;
static const uint MAX_SPOT_LIGHTS = KUKI_MAX_SPOT_LIGHTS;
// Taps the shadow filter takes to each side of the centre, so two is a five by five kernel and
// twenty-six shades between lit and shadowed. One tap gives two shades and a staircase for an edge;
// the eye picks out the steps of a three by three as banding, and stops being able to at five.
static const int SHADOW_FILTER_EXTENT = 2;
static const float SHADOW_FILTER_TAPS = (2 * SHADOW_FILTER_EXTENT + 1) * (2 * SHADOW_FILTER_EXTENT + 1);
// The soft shadows: how many taps look for what is casting the shadow, and how many then measure how
// much of the light it hides. Both are spread over a disc whose size is not known until the first
// lot have reported, and which may be a dozen times wider than the fixed kernel above -- so they are
// a spiral turned by a different angle in every pixel rather than a grid, which trades the bands a
// grid stretched that far would draw for a grain fine enough to pass for the penumbra's own texture.
//
// Sixteen is enough to find an occluder that covers any real part of the disc; missing one that
// covers a sixteenth of it loses a sixteenth of the shadow, at an edge that is being blurred anyway.
// Thirty-two is where the grain stops being visible at the widest radius allowed below.
static const int SHADOW_BLOCKER_TAPS = 16;
static const int SHADOW_PENUMBRA_TAPS = 32;
// The narrowest and widest a penumbra is filtered at, as a radius in shadow map texels.
//
// The floor is the fixed kernel's own reach, so a shadow is never harder than it was before any of
// this -- at the foot of what casts it, where the true penumbra is thinner than a texel, the map's
// grid is still what has to be hidden. The ceiling is what the tap count above can cover without
// the grain opening into dots, and what bounds the cost of the search: both loops read the same
// number of texels whatever the radius, so wider is not slower, only sparser.
static const float SHADOW_MIN_RADIUS = 2.5;
static const float SHADOW_MAX_RADIUS = 32.0;
// What is left for the slope bias to not have to cover: depth quantisation in the map itself. It can
// be this small only because the gradient carries the part that scales with the angle to the light.
static const float SHADOW_DEPTH_BIAS = 0.0002;
// How much of the map's depth range the receiver plane is allowed to claim its own depth crosses
// over the filter's footprint before that plane is thrown out. Past this it did not come from a
// single surface: a derivative quad straddling a crease holds two of them and solves for a plane
// through neither, and a surface the light grazes projects to so thin a sliver of the map that the
// solve divides by an area near zero. Both hand back a slope far larger than real geometry asks for.
static const float SHADOW_SLOPE_LIMIT = 0.01;
// The step taken along the surface normal before a sample is projected into the light, measured in
// shadow map texels at that sample's distance from it.
//
// This is what carries the bias where the plane is thrown out, and it is why throwing it out is
// safe. It moves the sample out of its own surface in the map rather than along the depth axis, so
// there is nothing in it that has to be retuned when the projection moves, and at a concave corner
// it steps away from both faces at once -- which is where the plane solve is least trustworthy.
// Below about one texel it stops clearing the map's own sampling grid; much above two and contact
// shadows begin to detach from what casts them.
static const float SHADOW_NORMAL_OFFSET = 1.5;
// Texels across one axis of a probe's visibility map, which the octahedron unfolds into a square of.
// Mirrors PROBE_DEPTH_RESOLUTION in dx_probe_volume.hpp, which is what sizes the stored map.
static const int PROBE_DEPTH_RESOLUTION = 16;
// The reconstruction's three tunables, and the three scales applied to what it returns, now live in
// `u_probeTuning` and `u_indirectScale`. See `IndirectLightingSettings`, which is where each one's
// default and the tradeoff it sits in the middle of are written out.
//
//   u_probeTuning.x  a step off the surface before the field is sampled, as a fraction of the local
//                    leaf's spacing. Too small and a concave corner reads as occluded from every
//                    probe around it, because the texel a probe samples towards the corner is a wide
//                    cone that catches the floor or the wall nearer than the point itself. Too large
//                    and the bounce detaches from the geometry it belongs to.
//   u_probeTuning.y  below this a probe is one the point can barely see, and a wall's worth of them
//                    adding up is how light gets through the wall. What is under the threshold is
//                    crushed against it, which drives them to nothing without putting a step into
//                    the weights above.
//   u_probeTuning.z  how sharply a partly-visible probe is disbelieved.
// Deepest the octree may subdivide. Mirrors PROBE_OCTREE_MAX_DEPTH in dx_probe_volume.hpp, and is
// read here only to put a leaf's depth on a nought-to-one scale for the cell view.
static const float PROBE_OCTREE_MAX_DEPTH = 5.0;
// Which shading step is written out in place of the finished pixel. Mirrors LightingDebugView in
// debug_view.hpp, whose declaration order these follow.
static const uint DEBUG_VIEW_NONE = 0;
static const uint DEBUG_VIEW_INDIRECT_DIFFUSE = 1;
static const uint DEBUG_VIEW_SKY_IRRADIANCE = 2;
static const uint DEBUG_VIEW_DIRECT_LIGHT = 3;
static const uint DEBUG_VIEW_SURFACE_OCCLUSION = 4;
static const uint DEBUG_VIEW_PROBE_VISIBILITY = 5;
static const uint DEBUG_VIEW_PROBE_WEIGHT = 6;
static const uint DEBUG_VIEW_PROBE_FALLBACK = 7;
static const uint DEBUG_VIEW_PROBE_CELL = 8;
static const uint DEBUG_VIEW_PROBE_BLEND = 9;
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
struct InstanceData {
  float4x4 model;
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
  float surface;
  uint samples;
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
cbuffer DrawConstants : register(b0) {
  float4 u_albedo;
  float4 u_specular;
  float4 u_emissive;
  float4 u_surface;
  float4 u_attenuation;
  float4 u_volume;
  uint u_textureMask;
  uint u_unlit;
  float u_alphaCutoff;
  uint u_alphaMode;
  // How far this surface is cel shaded, in `x`. See `MaterialFallback::toon`.
  float4 u_style;
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
  uint4 u_counts;
  uint4 u_flags;
  float4 u_probeVolume;
  uint4 u_probeCounts;
  uint4 u_debug;
  float4 u_indirectScale;
  float4 u_probeTuning;
  // The bands' count, softness, terminator and highlight size; the shaded side's tint and the rim's
  // strength; the rim's width. From the scene's `Stylization`.
  float4 u_toon;
  float4 u_toonShade;
  float4 u_toonRim;
};
Texture2D g_albedo : register(t0);
Texture2D g_normal : register(t1);
Texture2D g_metalness : register(t2);
Texture2D g_occlusion : register(t3);
Texture2D g_roughness : register(t4);
Texture2D g_specular : register(t5);
Texture2D g_emissive : register(t6);
Texture2D g_shadowMap : register(t7);
Texture2DArray g_spotShadowMap : register(t8);
StructuredBuffer<InstanceData> g_instances : register(t9);
StructuredBuffer<float4x4> g_bones : register(t10);
TextureCube g_irradiance : register(t11);
TextureCube g_prefilter : register(t12);
Texture2D g_brdfLUT : register(t13);
StructuredBuffer<Probe> g_probes : register(t14);
StructuredBuffer<OctreeNode> g_probeNodes : register(t15);
StructuredBuffer<uint> g_probeLookup : register(t16);
Texture2D g_sceneColor : register(t17);
SamplerState g_sampler : register(s0);
SamplerState g_shadowSampler : register(s1);
SamplerState g_clampSampler : register(s2);
struct VSInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texture0 : TEXCOORD0;
  float3 tangent : TANGENT;
};
struct VSSkinnedInput {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 texture0 : TEXCOORD0;
  float3 tangent : TANGENT;
  int4 boneIds : BLENDINDICES;
  float4 boneWeights : BLENDWEIGHT;
};
struct PSInput {
  float4 position : SV_POSITION;
  float3 worldPosition : TEXCOORD1;
  float3 normal : NORMAL;
  float3 tangent : TANGENT;
  float2 texture0 : TEXCOORD0;
  nointerpolation uint entityId : TEXCOORD2;
};
struct PSOutput {
  float4 color : SV_TARGET0;
  float4 entityId : SV_TARGET1;
};
PSInput BuildVertex(float4x4 world, float3 position, float3 normal, float3 tangent, float2 texture0, uint entityId) {
  PSInput output;
  float4 worldPosition = mul(world, float4(position, 1.0));
  output.position = mul(u_viewProjection, worldPosition);
  output.worldPosition = worldPosition.xyz;
  output.entityId = entityId;
  float3x3 model = (float3x3)world;
  float3 c0 = float3(model[0][0], model[1][0], model[2][0]);
  float3 c1 = float3(model[0][1], model[1][1], model[2][1]);
  float3 c2 = float3(model[0][2], model[1][2], model[2][2]);
  output.normal = normalize(cross(c1, c2) * normal.x + cross(c2, c0) * normal.y + cross(c0, c1) * normal.z);
  output.tangent = normalize(mul(model, tangent));
  output.texture0 = texture0;
  return output;
}
PSInput VSMain(VSInput input, uint instance : SV_InstanceID) {
  return BuildVertex(g_instances[instance].model, input.position, input.normal, input.tangent, input.texture0, g_instances[instance].entityId);
}
PSInput VSSkinned(VSSkinnedInput input, uint instance : SV_InstanceID) {
  float4x4 world = (float4x4)0;
  float total = 0.0;
  [unroll] for (int i = 0; i < 4; ++i)
    if (input.boneIds[i] >= 0 && input.boneWeights[i] > 0.0) {
      world += g_bones[input.boneIds[i]] * input.boneWeights[i];
      total += input.boneWeights[i];
    }
  if (total <= 0.0)
    world = g_instances[instance].model;
  return BuildVertex(world, input.position, input.normal, input.tangent, input.texture0, g_instances[instance].entityId);
}
float DistributionGGX(float3 N, float3 H, float R) {
  float a = R * R;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * denom * denom);
}
float GeometrySchlickGGX(float NdotV, float R) {
  float r = R + 1.0;
  float k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(float3 N, float3 V, float3 L, float R) {
  return GeometrySchlickGGX(max(dot(N, V), 0.0), R) * GeometrySchlickGGX(max(dot(N, L), 0.0), R);
}
float3 FresnelSchlick(float cosTheta, float3 F0) {
  return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float R) {
  return F0 + (max((1.0 - R).xxx, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}
float2 FallbackBRDF(float NdotV, float roughness) {
  return float2(NdotV, 1.0 - roughness);
}
float3 FallbackSky(float3 direction) {
  float t = saturate(normalize(direction).y * 0.5 + 0.5);
  return lerp(float3(0.6, 0.7, 0.9), float3(0.0, 0.1, 0.4), t);
}
float3 FallbackIrradiance(float3 direction) {
  return FallbackSky(direction) / PI;
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
  // One is the statistical answer and it leaks: a probe the point can half see still hands over half
  // its irradiance, and half a wall's worth of probes is a lit wall. Raising the exponent bends a
  // partial verdict towards nothing without touching a full one, since the value is already in
  // nought to one. `pow` rather than the cube this was written as, because which exponent is right
  // is a property of the scene's wall thickness against its probe spacing -- see
  // `IndirectLightingSettings::probeVisibilitySharpness`.
  // Refused outright where the probe measured this direction from the far side of something. The
  // Chebyshev bound above is a bound and nothing more: given an excess of a fraction of a probe
  // spacing -- which is all a wall of no thickness ever offers it -- it answers near one, and a
  // probe believed at near one through a wall is that wall lit from the wrong side. Orientation
  // answers the same question exactly and needs no thickness to do it, so it multiplies the bound
  // rather than being folded into the distance it is blind to.
  return pow(chebyshev, u_probeTuning.z) * (1.0 - moments.z);
}
bool FindProbeLeaf(float3 position, out OctreeNode node) {
  node = (OctreeNode)0;
  float3 local = (position - u_probeVolume.xyz) / u_probeVolume.w;
  if (any(local < 0.0) || any(local >= 1.0))
    return false;
  uint resolution = u_probeCounts.z;
  uint3 cell = min(uint3(local * float(resolution)), resolution - 1);
  uint leafIndex = g_probeLookup[cell.x + resolution * (cell.y + resolution * cell.z)];
  if (leafIndex >= u_probeCounts.y)
    return false;
  node = g_probeNodes[leafIndex];
  return node.leaf == 1;
}
// A colour standing for a leaf, which is all the cell view needs one to be.
//
// Hashed off the centre in units of the leaf's own size, so neighbouring cells differ and the same
// leaf keeps its colour from frame to frame -- a boundary seen to move is then the tree changing
// rather than the view flickering. Brightened with depth so that how far the octree refined reads
// at a glance instead of being lost in the hash.
float3 LeafColour(OctreeNode node) {
  float3 seed = node.center / max(node.extent, EPSILON);
  float3 hashed = frac(sin(float3(dot(seed, float3(127.1, 311.7, 74.7)), dot(seed, float3(269.5, 183.3, 246.1)), dot(seed, float3(113.5, 271.9, 124.6)))) * 43758.5453);
  return hashed * lerp(0.35, 1.0, saturate(float(node.depth) / PROBE_OCTREE_MAX_DEPTH));
}
// Everything the reconstruction worked out on the way to its answer.
//
// Only `irradiance` shades anything. The rest is carried out so the debug views can show a step that
// would otherwise live for the length of one expression: how much of the field the point was allowed
// to believe, what the corners summed to, whether the leak guard gave up, where in its cell the
// point sits. A member nothing reads is dropped by the compiler along with the arithmetic feeding
// it, so this costs nothing in the frames where no view is selected.
struct ProbeSample {
  float3 irradiance;
  /// @brief Share-weighted mean of the visibility the corners granted, nought to one.
  float visibility;
  /// @brief What the corner weights summed to, after the facing term and the crush.
  float weight;
  /// @brief One where no corner could see the point and plain interpolation stood in.
  float fallback;
  /// @brief Where in its leaf the point sits, eased, one component per axis.
  float3 blend;
  /// @brief A colour standing for the leaf the point landed in.
  float3 cell;
  /// @brief One where the point is inside the volume at all.
  float found;
};
ProbeSample SampleProbeVolume(float3 worldPosition, float3 N) {
  ProbeSample result = (ProbeSample)0;
  if (u_probeCounts.w == 0)
    return result;
  OctreeNode node;
  if (!FindProbeLeaf(worldPosition, node))
    return result;
  result.found = 1.0;
  // Sampled a step off the surface rather than on it. A point lying exactly on a wall is the same
  // distance from a probe as the wall is, so the visibility test cannot separate the two and reads
  // the surface as its own occluder. The normal is the direction that breaks that tie, and the step
  // is a fraction of the local leaf's spacing so it stays proportionate wherever the octree refined.
  //
  // Along the normal alone. This used to lean towards the viewer as well, which was meant to keep a
  // grazing surface from leaning back into the surface it stands on. What it actually did was make
  // where the field is read depend on where the camera is: orbiting a wall swung the step through
  // most of a right angle and slid the sample across the wall by a good part of the probe spacing,
  // so the bounce -- and the darkening the visibility test puts under an edge -- moved while the
  // scene stood still. Shading that follows the camera is worse than shading that is slightly wrong
  // and stays put, and it also put this out of step with `SampleVolume` in probe_trace.hlsl, which
  // has always biased along the normal and so disagreed with this about where a point is.
  float3 biased = worldPosition + N * (u_probeTuning.x * 2.0 * node.extent);
  OctreeNode biasedNode;
  if (FindProbeLeaf(biased, biasedNode))
    node = biasedNode;
  else
    biased = worldPosition;
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
  result.blend = t;
  result.cell = LeafColour(node);
  float3 total = 0.0;
  float weightSum = 0.0;
  float3 plainTotal = 0.0;
  float plainWeight = 0.0;
  float visibilityTotal = 0.0;
  float visibilityShare = 0.0;
  for (uint corner = 0; corner < 8; ++corner) {
    uint index = node.probes[corner >> 2][corner & 3];
    if (index >= u_probeCounts.x)
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
      float facing = dot(offset * rsqrt(length2), N) * 0.5 + 0.5;
      weight = facing * facing + 0.05;
    }
    float3 irradiance = EvaluateProbe(index, N);
    plainTotal += irradiance * weight * share;
    plainWeight += weight * share;
    // Kept rather than folded straight in, so the visibility view can report the verdict itself. It
    // is the one step of the reconstruction with no trace in the finished pixel: a corner that is
    // rejected and a corner that was never bright look identical once the eight are summed.
    float visibility = ProbeVisibility(index, probePosition, biased);
    visibilityTotal += visibility * share;
    visibilityShare += share;
    weight *= visibility;
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
  result.visibility = visibilityShare > EPSILON ? visibilityTotal / visibilityShare : 0.0;
  result.weight = weightSum;
  if (weightSum > EPSILON) {
    result.irradiance = total / (weightSum * PI);
    return result;
  }
  // Nothing could see this point -- it is inside geometry, or in a pocket none of the eight reach.
  // Interpolating anyway beats returning black, black being the failure this test exists to prevent
  // and a perverse thing to introduce while preventing it.
  result.fallback = 1.0;
  result.irradiance = plainWeight > 0.0 ? plainTotal / (plainWeight * PI) : 0.0;
  return result;
}
float3 VolumeAttenuation(float distance) {
  if (u_attenuation.w <= 0.0 || distance <= 0.0)
    return 1.0;
  float3 coefficient = -log(max(u_attenuation.rgb, EPSILON)) / u_attenuation.w;
  return exp(-coefficient * distance);
}
float3 SampleTransmission(float3 worldPosition, float3 N, float3 V, float3 A) {
  float ior = max(u_volume.z, 1.0);
  float thickness = max(u_volume.y, 0.0);
  float3 direction = refract(-V, N, 1.0 / ior);
  if (dot(direction, direction) < EPSILON)
    direction = -V;
  float4 clip = mul(u_viewProjection, float4(worldPosition + direction * thickness, 1.0));
  if (clip.w <= EPSILON)
    return 0.0;
  float2 uv = saturate(clip.xy / clip.w * float2(0.5, -0.5) + 0.5);
  float3 background = g_sceneColor.SampleLevel(g_clampSampler, uv, 0).rgb;
  return background * VolumeAttenuation(thickness) * A;
}
float3 GetNormalFromTexture(float2 texCoords, float3 normal, float3 tangent) {
  // Only red and green are read, and z is rebuilt from them. Normal maps are block-compressed to
  // BC5, which keeps two channels and leaves blue at zero, and a unit tangent-space normal makes the
  // third component redundant anyway, so this reads an uncompressed map identically.
  float2 tangentXY = g_normal.Sample(g_sampler, texCoords).xy * 2.0 - 1.0;
  float3 tangentNormal = float3(tangentXY, sqrt(max(0.0, 1.0 - dot(tangentXY, tangentXY))));
  float3 N = normalize(normal);
  float3 T = normalize(tangent);
  float3 B = normalize(cross(N, T));
  return normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
}
float3 ProjectToShadowMap(float4 positionLight) {
  float3 projected = positionLight.xyz / positionLight.w;
  return float3(projected.x * 0.5 + 0.5, projected.y * -0.5 + 0.5, projected.z);
}
bool OutsideShadowMap(float3 projected) {
  return projected.z < 0.0 || projected.z > 1.0 || any(projected.xy < 0.0) || any(projected.xy > 1.0);
}
// The world-space width of one shadow map texel at a point, read off the light's own matrix so that
// nothing has to be uploaded beside it. A world step reaches clip space through the matrix's first
// two rows and normalised device space through those divided by w, so the world distance one texel
// spans is its share of the two device units the map covers, scaled back up by w. For the
// directional light's orthographic matrix w is one and this is constant across the scene; for a spot
// it grows with distance from the light, which is what a perspective shadow map does to its texels.
float ShadowTexelWorldSize(float4x4 lightViewProjection, float3 worldPosition, float2 size) {
  float w = max(mul(lightViewProjection, float4(worldPosition, 1.0)).w, EPSILON);
  float2 clipPerWorld = float2(length(lightViewProjection[0].xyz), length(lightViewProjection[1].xyz));
  return max(2.0 * w / max(size.x * clipPerWorld.x, EPSILON), 2.0 * w / max(size.y * clipPerWorld.y, EPSILON));
}
// Where the map is asked about, which is not quite where the surface is.
//
// The map holds one depth per texel, and the surface under a texel spans a range of depths, so a
// point taken on the surface itself sits behind its own recorded depth over half of every texel it
// falls in -- the surface shadowing itself. Stepping out along the normal first lifts the point
// clear of that range. The step has to be longer the more depth a texel holds, which is the sine of
// the angle between surface and light: nothing head on, most at a graze.
float3 ShadowSamplePosition(float4x4 lightViewProjection, float3 worldPosition, float3 N, float3 L, float2 size) {
  float NdotL = saturate(dot(N, L));
  float sine = sqrt(saturate(1.0 - NdotL * NdotL));
  return worldPosition + N * (SHADOW_NORMAL_OFFSET * sine * ShadowTexelWorldSize(lightViewProjection, worldPosition, size));
}
// How the receiver's own depth changes per unit step across the shadow map, recovered from the
// screen-space derivatives of its shadow-map coordinate.
//
// A shadow map holds one depth per texel and the filter below reads twenty-five of them, but a
// surface at any angle to the light sits at a different depth under every one. Comparing them all
// against the depth at the centre tap is what makes a sloped surface shadow itself in stripes. The
// usual answer pushes the comparison back by a constant scaled by the angle, which trades the
// stripes for light leaking out from under the surface, and has to be retuned whenever the
// projection moves, because that constant lives in a post-projection depth that is not linear in
// distance -- the reason the old `0.05 * (1 - N.L)` erased the shadows on these walls entirely while
// leaving the floor's intact. Solving for the plane the receiver actually lies on instead lets each
// tap be compared against the receiver's depth *at that tap*. It is exact for a flat surface at any
// angle to the light, and there is nothing in it to tune -- where it holds at all.
float2 ShadowDepthGradient(float3 projected, float2 texel) {
  float3 dx = ddx(projected);
  float3 dy = ddy(projected);
  float area = dx.x * dy.y - dx.y * dy.x;
  // A relative floor rather than an absolute one. How large this area is in the first place is set
  // by how much of the map a pixel covers, which moves with the distance to the light and with the
  // map's resolution, so a fixed epsilon either never fires or fires across a whole surface. What it
  // has to catch is the solve going singular -- the quad's two steps across the map running
  // parallel, which is what a surface the light grazes does to them.
  if (abs(area) < EPSILON * max(length(dx.xy) * length(dy.xy), EPSILON))
    return 0.0;
  float2 gradient = float2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / area;
  // The largest bias the furthest tap will ask this gradient for. Over the limit the plane is
  // dropped rather than clamped to it: a clamped slope keeps the sign it was given, and on the taps
  // where that sign is negative it pulls the comparison in front of the occluder and calls the tap
  // lit. That is the bright line through a shadow along a wall-to-ceiling crease, and it moves with
  // the camera because which derivative quads straddle the crease is a screen-space fact. The normal
  // offset is what holds the bias up once the plane is gone.
  if (SHADOW_FILTER_EXTENT * dot(texel, abs(gradient)) > SHADOW_SLOPE_LIMIT)
    return 0.0;
  return gradient;
}
// One depth out of either map: the directional light's for a layer below nought, and that layer of
// the spot lights' array otherwise. The two are different kinds of texture and cannot be handed to
// one function as an argument, and everything below is the same arithmetic for both.
float ShadowMapDepth(int layer, float2 uv) {
  if (layer < 0)
    return g_shadowMap.SampleLevel(g_shadowSampler, uv, 0).r;
  return g_spotShadowMap.SampleLevel(g_shadowSampler, float3(uv, layer), 0).r;
}
// The shadow of a light with no size: the fixed kernel, the same width everywhere. What every
// shadow was before lights had a size, and still what one gets when its light is given none.
float FixedShadowAmount(int layer, float3 projected, float2 gradient, float2 texel) {
  float shadow = 0.0;
  for (int y = -SHADOW_FILTER_EXTENT; y <= SHADOW_FILTER_EXTENT; ++y)
    for (int x = -SHADOW_FILTER_EXTENT; x <= SHADOW_FILTER_EXTENT; ++x) {
      float2 offset = float2(x, y) * texel;
      float slope = dot(offset, gradient);
      shadow += projected.z + slope - SHADOW_DEPTH_BIAS > ShadowMapDepth(layer, projected.xy + offset) ? 1.0 : 0.0;
    }
  return shadow / SHADOW_FILTER_TAPS;
}
// A point of a disc of unit radius, the `index`th of `count` spread evenly over it along a spiral.
// The square root is what makes them even by area, and the golden angle between one and the next is
// what keeps any two from lining up, whatever the count.
float2 ShadowDisc(int index, int count, float rotation) {
  float angle = index * 2.39996323 + rotation;
  return sqrt((index + 0.5) / count) * float2(cos(angle), sin(angle));
}
// An angle for the spiral that differs from each pixel to its neighbours and averages out over any
// few of them: interleaved gradient noise, which is built to have that property along both axes.
float ShadowRotation(float2 pixel) {
  return 2.0 * PI * frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}
// The shadow of a light that has a size: percentage-closer soft shadows.
//
// A light with a size is hidden a part at a time. How wide the band is across which that happens
// depends on how far the thing casting the shadow is from the thing receiving it -- similar
// triangles, with the light at one end -- so a shadow is sharp where a foot meets the floor and
// spreads the further it falls. A fixed kernel cannot say that, because the width it would need is
// different at every pixel and is not known until the occluder has been found.
//
// So this looks for it first. The first loop reads the map around the point and averages the depth
// of everything nearer the light than the receiver; the gap between that and the receiver's own
// depth gives the width of the penumbra here, and the second loop filters over exactly that. A
// point with nothing in front of it is fully lit and costs the first loop only.
//
// `gap` is how much further the receiver is than the occluder as a multiple of what `spread` was
// measured in, and the two differ between the kinds of light. Under the directional light's
// orthographic map depth is linear in distance, so the gap is a plain difference of depths. Under a
// spot's perspective map it is not, and the penumbra goes as the ratio of the two distances from the
// light rather than as their difference -- so `depthLine` carries the two numbers that turn a depth
// back into a distance, `w = y / (z - x)`, and the gap is taken between distances.
//
// The search is as wide as the widest penumbra it could find, since an occluder that far from the
// receiver is that far off to the side in the map when the light's nearest edge is behind it.
float PenumbraShadowAmount(int layer, float3 projected, float2 gradient, float2 texel, float2 pixel, float search, float spread, float2 depthLine, bool perspective) {
  float rotation = ShadowRotation(pixel);
  float blockers = 0.0;
  float blockerDepth = 0.0;
  for (int b = 0; b < SHADOW_BLOCKER_TAPS; ++b) {
    float2 offset = ShadowDisc(b, SHADOW_BLOCKER_TAPS, rotation) * (search * texel);
    float mapDepth = ShadowMapDepth(layer, projected.xy + offset);
    // The same test the filter makes, plane and bias and all. A looser one here would find the
    // receiver's own surface as its occluder at a distance of nothing, and report no penumbra.
    if (projected.z + dot(offset, gradient) - SHADOW_DEPTH_BIAS > mapDepth) {
      blockerDepth += perspective ? depthLine.y / (mapDepth - depthLine.x) : mapDepth;
      blockers += 1.0;
    }
  }
  if (blockers == 0.0)
    return 0.0;
  blockerDepth /= blockers;
  float receiverDepth = perspective ? depthLine.y / (projected.z - depthLine.x) : projected.z;
  float gap = perspective ? (receiverDepth - blockerDepth) / max(blockerDepth, EPSILON) : receiverDepth - blockerDepth;
  float radius = clamp(spread * max(gap, 0.0), SHADOW_MIN_RADIUS, SHADOW_MAX_RADIUS);
  float shadow = 0.0;
  for (int p = 0; p < SHADOW_PENUMBRA_TAPS; ++p) {
    float2 offset = ShadowDisc(p, SHADOW_PENUMBRA_TAPS, rotation) * (radius * texel);
    shadow += projected.z + dot(offset, gradient) - SHADOW_DEPTH_BIAS > ShadowMapDepth(layer, projected.xy + offset) ? 1.0 : 0.0;
  }
  return shadow / SHADOW_PENUMBRA_TAPS;
}
float DirectionalShadowAmount(float3 worldPosition, float3 N, float3 L, float2 pixel) {
  if (u_counts.w == 0)
    return 0.0;
  float2 size;
  g_shadowMap.GetDimensions(size.x, size.y);
  float2 texel = 1.0 / size;
  float3 sampled = ShadowSamplePosition(u_lightViewProjection, worldPosition, N, L, size);
  float4 positionLight = mul(u_lightViewProjection, float4(sampled, 1.0));
  float3 projected = ProjectToShadowMap(positionLight);
  // taken before anything returns, since a derivative is only meaningful where the whole quad took
  // the same path to it
  float2 gradient = ShadowDepthGradient(projected, texel);
  if (positionLight.w <= 0.0 || OutsideShadowMap(projected))
    return 0.0;
  // The tangent of half the angle the light subtends, which is how far a penumbra's edge moves
  // sideways for each unit the shadow falls. Nought is a light with no size.
  float slope = u_directionalIntensity.y;
  if (slope <= 0.0)
    return FixedShadowAmount(-1, projected, gradient, texel);
  // Texels of penumbra radius for each unit of the map's depth between occluder and receiver. The
  // matrix's third row is how much depth a world step along the light is worth, read off it for the
  // reason `ShadowTexelWorldSize` reads the first two: so nothing has to be uploaded beside it.
  float worldPerDepth = 1.0 / max(length(u_lightViewProjection[2].xyz), EPSILON);
  float spread = slope * worldPerDepth / ShadowTexelWorldSize(u_lightViewProjection, sampled, size);
  // Nothing can be casting on this point from further back than the map's near plane, which is
  // where depth is nought, so the receiver's own depth is the largest gap there is to look across.
  float search = clamp(spread * projected.z, SHADOW_MIN_RADIUS, SHADOW_MAX_RADIUS);
  return PenumbraShadowAmount(-1, projected, gradient, texel, pixel, search, spread, 0.0, false);
}
float SpotShadowAmount(uint index, float3 worldPosition, float3 N, float3 L, float2 pixel) {
  if (index >= u_flags.y)
    return 0.0;
  float3 size;
  g_spotShadowMap.GetDimensions(size.x, size.y, size.z);
  float2 texel = 1.0 / size.xy;
  float4x4 lightViewProjection = u_spotLightViewProjection[index];
  float3 sampled = ShadowSamplePosition(lightViewProjection, worldPosition, N, L, size.xy);
  float4 positionLight = mul(lightViewProjection, float4(sampled, 1.0));
  float3 projected = ProjectToShadowMap(positionLight);
  float2 gradient = ShadowDepthGradient(projected, texel);
  if (positionLight.w <= 0.0 || OutsideShadowMap(projected))
    return 0.0;
  // The radius of the light itself, in scene units. Nought is a light with no size.
  float sourceRadius = u_spotLights[index].cutoff.z;
  if (sourceRadius <= 0.0)
    return FixedShadowAmount(int(index), projected, gradient, texel);
  // A perspective map's depth as a function of distance from the light is `x + y / w`, and both
  // numbers are in the matrix. Clip depth is a straight line in clip w, whose slope is the third
  // row's share of the fourth -- the two rows point the same way, along the light's axis -- and
  // whose intercept is whatever is left over at this point.
  float2 depthLine;
  depthLine.x = dot(lightViewProjection[2].xyz, lightViewProjection[3].xyz) / max(dot(lightViewProjection[3].xyz, lightViewProjection[3].xyz), EPSILON);
  depthLine.y = positionLight.z - depthLine.x * positionLight.w;
  // Texels of penumbra radius when the receiver is twice as far from the light as the occluder:
  // the light's own radius, measured in this point's texels.
  float spread = sourceRadius / ShadowTexelWorldSize(lightViewProjection, sampled, size.xy);
  // The nearest an occluder can be is the map's near plane, where depth is nought.
  float nearest = depthLine.y / -depthLine.x;
  float search = clamp(spread * (positionLight.w - nearest) / max(nearest, EPSILON), SHADOW_MIN_RADIUS, SHADOW_MAX_RADIUS);
  return PenumbraShadowAmount(int(index), projected, gradient, texel, pixel, search, spread, depthLine, true);
}
float3 DirectContribution(float3 L, float3 radiance, float3 specularRadiance, float3 F0, float3 A, float3 N, float M, float R, float3 V, float diffuseScale) {
  float3 H = normalize(V + L);
  float NdotL = max(dot(N, L), 0.0);
  float NDF = DistributionGGX(N, H, R);
  float G = GeometrySmith(N, V, L, R);
  float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
  float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
  float3 kD = (1.0 - F) * (1.0 - M) * diffuseScale;
  float3 diffuse = (A / PI) * radiance;
  float3 specular = (NDF * G * F / denominator) * specularRadiance;
  return (kD * diffuse + specular) * NdotL;
}
// A cosine stepped into the scene's bands: nought on the shaded side of the terminator, one at the
// top, and as many evenly spaced tones between as were asked for. Each step is a smoothstep a few
// hundredths wide rather than a hard edge, which is the difference between a band that holds still on
// a turning arm and one that crawls along it a pixel at a time.
float CelBands(float cosine) {
  float bands = u_toon.x;
  float soft = u_toon.y;
  float terminator = u_toon.z;
  float total = 0.0;
  int edges = max(int(bands + 0.5) - 1, 1);
  for (int k = 0; k < edges; ++k) {
    float edge = terminator + (1.0 - terminator) * float(k) / float(edges + 1);
    total += smoothstep(edge - soft, edge + soft, cosine);
  }
  return total / float(edges);
}
// One light as a cel would draw it. The diffuse is the albedo at full strength wherever the band says
// lit, rather than falling off with the cosine: in a drawing a lit plane is one colour. The shadow
// map is stepped as well, so the edge of a cast shadow is a band edge like any other. The highlight
// is a hard disc, as large as the scene asks and dimmer the rougher the surface.
float3 ToonContribution(float3 L, float3 radiance, float3 specularRadiance, float unshadowed, float3 A, float3 N, float3 V, float R, out float cel) {
  float soft = u_toon.y;
  cel = CelBands(dot(N, L)) * smoothstep(0.5 - soft * 4.0, 0.5 + soft * 4.0, unshadowed);
  float3 diffuse = (A / PI) * radiance * cel;
  float size = u_toon.w;
  float highlight = 0.0;
  if (size > 0.0) {
    float NdotH = saturate(dot(N, normalize(V + L)));
    float threshold = 1.0 - size * 0.5;
    highlight = smoothstep(threshold - soft * 0.2, threshold + soft * 0.2, NdotH) * (1.0 - R) * (1.0 - R);
  }
  return diffuse + specularRadiance * (highlight * cel * 0.35);
}
PSOutput PSMain(PSInput input) {
  PSOutput output;
  bool useAlbedo = (u_textureMask & 0x1) != 0;
  bool useNormal = (u_textureMask & 0x2) != 0;
  bool useMetalness = (u_textureMask & 0x4) != 0;
  bool useOcclusion = (u_textureMask & 0x8) != 0;
  bool useRoughness = (u_textureMask & 0x10) != 0;
  bool useSpecular = (u_textureMask & 0x20) != 0;
  bool useEmissive = (u_textureMask & 0x40) != 0;
  float4 A = useAlbedo ? g_albedo.Sample(g_sampler, input.texture0) : u_albedo;
  if (useAlbedo)
    A *= u_albedo;
  if (u_alphaMode == ALPHA_MODE_MASK && A.a < u_alphaCutoff)
    discard;
  float alpha = u_alphaMode == ALPHA_MODE_OPAQUE ? 1.0 : A.a;
  uint id = input.entityId;
  output.entityId = float4((id & 0xFF) / 255.0, ((id >> 8) & 0xFF) / 255.0, ((id >> 16) & 0xFF) / 255.0, 1.0);
  if (u_unlit != 0) {
    // Nothing here took part in the lighting at all. Under a debug view its albedo would be a colour
    // on the screen meaning something entirely different from every other colour on the screen.
    output.color = u_debug.x != DEBUG_VIEW_NONE ? float4(0.0, 0.0, 0.0, alpha) : float4(A.rgb, alpha);
    return output;
  }
  // Kept apart from N because the shadow lookup steps along the interpolated surface normal, not the
  // one a normal map bends. The map holds no record of that detail, and a step whose length jumps
  // from pixel to pixel is the one thing the receiver plane solve cannot survive.
  float3 GN = normalize(input.normal);
  float3 N = useNormal ? GetNormalFromTexture(input.texture0, GN, input.tangent) : GN;
  float4 S = useSpecular ? g_specular.Sample(g_sampler, input.texture0) : u_specular;
  float4 E = useEmissive ? g_emissive.Sample(g_sampler, input.texture0) : u_emissive;
  // The material's own occlusion map, and now the whole of the factor every indirect term is
  // scaled by. The screen space estimate that used to multiply it is gone: it cancelled its sample
  // rotation against a window fixed at four pixels, which only holds where the occlusion is smooth
  // across four pixels, so a crease seen from far enough away printed the rotation into the image.
  float O = useOcclusion ? g_occlusion.Sample(g_sampler, input.texture0).r : u_surface.y;
  float R = useRoughness ? g_roughness.Sample(g_sampler, input.texture0).g : u_surface.z;
  float M = useMetalness ? g_metalness.Sample(g_sampler, input.texture0).b : u_surface.x;
  float3 V = normalize(u_viewPosition.xyz - input.worldPosition);
  float3 reflectDirection = reflect(-V, N);
  float dielectric = pow((max(u_volume.z, 1.0) - 1.0) / (max(u_volume.z, 1.0) + 1.0), 2.0);
  float3 F0 = lerp(dielectric * S.rgb, A.rgb, M);
  float NdotV = max(dot(N, V), 0.0);
  float3 F = FresnelSchlickRoughness(NdotV, F0, R);
  float3 kD = (1.0 - F) * (1.0 - M);
  float transmissionWeight = saturate(u_volume.x) * (1.0 - M);
  float diffuseScale = 1.0 - transmissionWeight;
  float3 transmitted = transmissionWeight > 0.0 ? kD * transmissionWeight * SampleTransmission(input.worldPosition, N, V, A.rgb) : 0.0;
  ProbeSample probes = SampleProbeVolume(input.worldPosition, N);
  // Scaled here rather than inside the reconstruction, so the debug view above still reports what
  // the field actually holds. A view that moved with the slider could not answer the question it is
  // for -- whether the bounce is too dim because the field is dark or because it is being turned
  // down -- since both would look identical.
  float3 bounced = kD * diffuseScale * probes.irradiance * u_indirectScale.x;
  float3 ambient;
  // The reflection of the sky alone, kept apart so the cel half can take most of it away: a drawn
  // surface does not mirror its surroundings, and a cel band with the sky's gradient across it reads
  // as a mistake.
  float3 ambientSpecular = 0.0;
  // Hoisted out of the branch below so a debug view can read it. Nought where there is no sky, which
  // is the honest answer rather than a missing one.
  float3 skyIrradiance = 0.0;
  if (u_flags.x != 0) {
    bool precomputed = u_flags.z != 0;
    float3 irradiance = (precomputed ? g_irradiance.SampleLevel(g_sampler, N, 0).rgb : FallbackIrradiance(N)) * u_indirectScale.y;
    skyIrradiance = irradiance;
    float3 reflected = precomputed ? g_prefilter.SampleLevel(g_sampler, reflectDirection, R * MAX_REFLECTION_LOD).rgb : FallbackSky(reflectDirection);
    float2 brdf = precomputed ? g_brdfLUT.SampleLevel(g_sampler, float2(NdotV, R), 0).rg : FallbackBRDF(NdotV, R);
    float3 specular = reflected * (F * brdf.x + brdf.y);
    ambientSpecular = specular * O;
    ambient = ((kD * diffuseScale * irradiance + bounced) * A.rgb + specular) * O;
  } else if (u_counts.z != 0)
    ambient = (u_directionalAmbient.rgb * diffuseScale + bounced) * A.rgb * O;
  else if (u_probeCounts.w != 0)
    ambient = bounced * A.rgb * O;
  else
    ambient = u_indirectScale.z * diffuseScale * A.rgb * O;
  float3 Lo = 0.0;
  // The cel half of the shading, built beside the physical half and blended with it at the end by
  // how far the material asks for bands. Nothing here runs for a material that asks for none.
  float toon = u_style.x;
  float3 LoToon = 0.0;
  float cel = 0.0;
  // How lit the surface is by the sun, in bands. One with no sun, so that the shaded side's tint is
  // what a surface gets for facing away from a light, and not for there being no light to face.
  float sunCel = 1.0;
  float3 sunRadiance = 0.0;
  if (u_counts.z != 0) {
    float3 L = normalize(-u_directionalDirection.xyz);
    float intensity = u_directionalIntensity.x;
    float shadow = DirectionalShadowAmount(input.worldPosition, GN, L, input.position.xy);
    Lo += (1.0 - shadow) * DirectContribution(L, u_directionalDiffuse.rgb * intensity, u_directionalSpecular.rgb * intensity, F0, A.rgb, N, M, R, V, diffuseScale);
    if (toon > 0.0) {
      LoToon += ToonContribution(L, u_directionalDiffuse.rgb * intensity, u_directionalSpecular.rgb * intensity, 1.0 - shadow, A.rgb, N, V, R, cel);
      sunCel = cel;
      sunRadiance = u_directionalDiffuse.rgb * intensity / PI;
    }
  }
  for (uint p = 0; p < min(u_counts.x, MAX_POINT_LIGHTS); ++p) {
    float3 offset = u_pointLights[p].position.xyz - input.worldPosition;
    float distance = length(offset);
    float3 L = offset / max(distance, EPSILON);
    float3 a = u_pointLights[p].attenuation.xyz;
    float intensity = u_pointLights[p].attenuation.w;
    float attenuation = intensity / (a.x + a.y * distance + a.z * distance * distance);
    Lo += DirectContribution(L, u_pointLights[p].diffuse.rgb * attenuation, u_pointLights[p].specular.rgb * attenuation, F0, A.rgb, N, M, R, V, diffuseScale);
    if (toon > 0.0)
      LoToon += ToonContribution(L, u_pointLights[p].diffuse.rgb * attenuation, u_pointLights[p].specular.rgb * attenuation, 1.0, A.rgb, N, V, R, cel);
  }
  for (uint s = 0; s < min(u_counts.y, MAX_SPOT_LIGHTS); ++s) {
    float3 offset = u_spotLights[s].position.xyz - input.worldPosition;
    float distance = length(offset);
    float3 L = offset / max(distance, EPSILON);
    float3 a = u_spotLights[s].attenuation.xyz;
    float intensity = u_spotLights[s].attenuation.w;
    float theta = dot(L, normalize(-u_spotLights[s].direction.xyz));
    float cone = saturate((theta - u_spotLights[s].cutoff.y) / max(u_spotLights[s].cutoff.x - u_spotLights[s].cutoff.y, EPSILON));
    float attenuation = intensity * cone / (a.x + a.y * distance + a.z * distance * distance);
    float shadow = SpotShadowAmount(s, input.worldPosition, GN, L, input.position.xy);
    Lo += (1.0 - shadow) * DirectContribution(L, u_spotLights[s].diffuse.rgb * attenuation, u_spotLights[s].specular.rgb * attenuation, F0, A.rgb, N, M, R, V, diffuseScale);
    if (toon > 0.0)
      LoToon += ToonContribution(L, u_spotLights[s].diffuse.rgb * attenuation, u_spotLights[s].specular.rgb * attenuation, 1.0 - shadow, A.rgb, N, V, R, cel);
  }
  // One step of the shading, promoted to the output.
  //
  // Everything below was computed above for the finished pixel, so a view costs a branch and no
  // arithmetic, and the branch is uniform across the draw. It runs after the entity id is written so
  // that picking still works while a view is up: the views are for looking at a scene, not for
  // leaving the editor unable to select anything in it.
  if (u_debug.x != DEBUG_VIEW_NONE) {
    float3 shown = 0.0;
    switch (u_debug.x) {
    case DEBUG_VIEW_INDIRECT_DIFFUSE:
      shown = probes.irradiance;
      break;
    case DEBUG_VIEW_SKY_IRRADIANCE:
      shown = skyIrradiance;
      break;
    case DEBUG_VIEW_DIRECT_LIGHT:
      shown = Lo;
      break;
    case DEBUG_VIEW_SURFACE_OCCLUSION:
      shown = O;
      break;
    case DEBUG_VIEW_PROBE_VISIBILITY:
      shown = probes.visibility;
      break;
    case DEBUG_VIEW_PROBE_WEIGHT:
      shown = probes.weight;
      break;
    case DEBUG_VIEW_PROBE_FALLBACK:
      // Three states rather than two, because "outside the volume" and "inside it and past saving"
      // are different problems and look the same in every other view.
      shown = probes.found == 0.0 ? float3(0.0, 0.0, 0.4) : (probes.fallback > 0.0 ? float3(1.0, 0.0, 0.0) : float3(0.15, 0.15, 0.15));
      break;
    case DEBUG_VIEW_PROBE_CELL:
      shown = probes.cell;
      break;
    case DEBUG_VIEW_PROBE_BLEND:
      shown = probes.blend;
      break;
    }
    output.color = float4(shown, alpha);
    return output;
  }
  float3 shaded = ambient + Lo;
  if (toon > 0.0) {
    // The ambient is kept, so a figure still takes the colour of the sky it stands under, but the
    // shaded side has the scene's tint laid over it -- the shadow is a colour, not a darkness.
    float3 ambientDiffuse = ambient - ambientSpecular;
    float3 toonAmbient = ambientDiffuse * lerp(u_toonShade.rgb, 1.0, sunCel) + ambientSpecular * 0.2;
    // The rim: a band of light along the silhouette, lit by the sun where there is one and by the
    // sky where there is not, stronger on the side the light comes from.
    float rimWidth = u_toonRim.x;
    float rimEdge = smoothstep(1.0 - rimWidth - u_toon.y, 1.0 - rimWidth + u_toon.y, 1.0 - NdotV);
    float3 rimLight = any(sunRadiance > 0.0) ? sunRadiance : max(ambientDiffuse, 0.0);
    float3 rim = u_toonShade.w * rimEdge * (0.35 + 0.65 * sunCel) * rimLight * lerp(A.rgb, 1.0, 0.5);
    shaded = lerp(shaded, toonAmbient + LoToon + rim, toon);
  }
  output.color = float4(shaded + transmitted + E.rgb, alpha);
  return output;
}
