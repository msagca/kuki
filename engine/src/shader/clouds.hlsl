// A layer of cloud in the atmosphere `atmosphere.hlsl` describes, marched from the camera every
// frame instead of being baked into the sky with the rest of it.
//
// The medium is the one everybody's is: a shape noise that says where cloud is, a detail noise that
// erodes its edges, a height gradient that gives the layer a base and a top, and a coverage
// threshold subtracted before either is used -- so raising coverage grows clouds together rather
// than thickening each one, which is what cloud cover does. The lighting is Hillaire's and
// Schneider's between them: a short march towards the sun for the cloud's own shadow, a dual
// Henyey-Greenstein phase so a cloud lit from behind has a rim and one lit from in front does not,
// the powder term for the dark approach to an edge, and several orders of scattering approximated
// by summing octaves of decreasing extinction.
//
// Geometry, units and conventions are `atmosphere.hlsl`'s exactly: the planet is centred at the
// origin, the viewer sits on +Y, everything is scaled so the planet's radius is one, and the
// scattering the march produces is in the same radiance the sky around it is. That is not tidiness.
// The cloud is composited over a sky that was marched in those units, lit by a sun read out of that
// sky's transmittance table, and filled in by that sky's cubemap -- three places where a second
// convention would show as a cloud that does not belong to the sky behind it.
//
// What this file does not do is put a cloud's shadow on the ground. The scene is shaded before this
// pass runs and does not read anything here; what `u_shadowStrength` reaches is the air, through
// the one texel `CSSunTransmittance` writes -- see the note there.
static const float PI = 3.14159265359;
static const float EPSILON = 1.0e-6;
/// @brief Orders of scattering the march approximates, each cheaper and flatter than the last.
///
/// Single scattering alone leaves a cloud with a bright edge and a black middle, because everything
/// that reached the interior did so by bouncing and single scattering is by definition what did
/// not. Summing octaves with extinction, scattering and phase eccentricity each raised to a power
/// is Hillaire's approximation of the rest: the later octaves reach further into the cloud, scatter
/// less, and point less, which between them is what light that has bounced several times does.
static const uint SCATTERING_OCTAVES = 3;
static const float OCTAVE_EXTINCTION = 0.5;
static const float OCTAVE_SCATTERING = 0.5;
static const float OCTAVE_ECCENTRICITY = 0.5;
/// @brief Mip of the sky cubemap the ambient term is read from.
///
/// High enough that what comes back is the average of a whole quadrant of sky rather than whatever
/// happens to be in one direction. The cubemap is 1024 a side with a full chain, so six levels is a
/// sixteen-texel face -- a blurred sky, which is what filling a cloud's shaded side wants.
static const float AMBIENT_MIP = 6.0;
/// @brief The range the dilated shape field occupies, which is nothing like its nominal one.
///
/// Measured from the construction rather than tuned by eye -- see the note in `CloudDensity`, which
/// is also the only place these are read.
static const float BASE_FLOOR = 0.55;
static const float BASE_CEILING = 0.95;
cbuffer CloudConstants : register(b0) {
  float4x4 u_inverseViewProjection;
  float3 u_sunDirection;
  float u_sunIntensity;
  float3 u_windOffset;
  float u_time;
  float u_bottomRadius;
  float u_topRadius;
  float u_viewAltitude;
  float u_kilometres;
  float u_cloudBottom;
  float u_cloudTop;
  float u_coverage;
  float u_density;
  float u_shapeScale;
  float u_detailScale;
  float u_detailStrength;
  float u_anisotropy;
  float u_backscatter;
  float u_powder;
  float u_ambient;
  float u_scatteringScale;
  float u_shadowStrength;
  uint u_steps;
  uint u_lightSteps;
  uint u_noiseSize;
  uint u_width;
  uint u_height;
  uint u_frame;
  uint u_padding0;
};
Texture3D g_shapeNoise : register(t0);
Texture3D g_detailNoise : register(t1);
Texture2D g_transmittance : register(t2);
TextureCube g_sky : register(t3);
Texture2D g_depth : register(t4);
RWTexture3D<float4> g_volume : register(u0);
RWTexture2D<float4> g_lut : register(u1);
/// @brief Linear and wrapping, which is what the two noise volumes need and only they.
///
/// The layer drifts without limit, so the coordinates handed to the volumes grow without limit too
/// and rely on the wrap to bring them back. The two tables below must not wrap: the transmittance
/// table's edges are the horizon and the top of the atmosphere, and a wrapped sample there reads the
/// opposite extreme of the sky -- which is why they are given a sampler of their own rather than
/// sharing this one.
SamplerState g_sampler : register(s0);
SamplerState g_pointSampler : register(s1);
SamplerState g_clampSampler : register(s2);
// ---------------------------------------------------------------------------------------------
// Noise
//
// Built once into two volumes rather than evaluated in the march, which is the difference between
// this pass costing a texture fetch per step and costing forty. Both are tileable, and that is not
// optional here: the layer drifts on the wind without limit, so a field that did not repeat would
// need coordinates that grow until a float cannot hold them.
// ---------------------------------------------------------------------------------------------
float Hash(float3 cell) {
  // Three large odd multipliers and a fract, which is the cheapest hash that does not show its
  // lattice. The values are the usual ones; any three coprime constants of this size work.
  float3 p = frac(cell * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return frac((p.x + p.y) * p.z);
}
float3 HashPoint(float3 cell) {
  return float3(Hash(cell), Hash(cell + 17.3), Hash(cell + 41.7));
}
/// @brief Worley noise over a wrapped lattice, returned inverted so that one is the cell's centre.
///
/// Inverted because what a cloud wants is billows -- rounded lumps with creases between them --
/// and Worley as defined is the distance to the nearest feature point, which is the creases with
/// lumps between. Everything downstream reads the inverted form, so it is inverted here once.
float Worley(float3 position, float cells) {
  float3 scaled = position * cells;
  float3 base = floor(scaled);
  float3 offset = scaled - base;
  float nearest = 1.0;
  for (int z = -1; z <= 1; ++z)
    for (int y = -1; y <= 1; ++y)
      for (int x = -1; x <= 1; ++x) {
        float3 neighbour = float3(x, y, z);
        // Wrapped before hashing, so the cell across the far edge of the volume is the same cell as
        // the one before the near edge and the feature points agree across the seam.
        float3 cell = fmod(base + neighbour + cells, cells);
        float3 feature = neighbour + HashPoint(cell) - offset;
        nearest = min(nearest, dot(feature, feature));
      }
  return 1.0 - saturate(sqrt(nearest));
}
float ValueNoise(float3 position, float cells) {
  float3 scaled = position * cells;
  float3 base = floor(scaled);
  float3 offset = scaled - base;
  // Quintic rather than smoothstep: its second derivative vanishes at the ends too, so the lattice
  // does not show as faint creases once several octaves are summed.
  float3 weight = offset * offset * offset * (offset * (offset * 6.0 - 15.0) + 10.0);
  float result = 0.0;
  for (int z = 0; z <= 1; ++z)
    for (int y = 0; y <= 1; ++y)
      for (int x = 0; x <= 1; ++x) {
        float3 corner = float3(x, y, z);
        float3 cell = fmod(base + corner + cells, cells);
        float3 blend = lerp(1.0 - weight, weight, corner);
        result += Hash(cell) * blend.x * blend.y * blend.z;
      }
  return result;
}
float ValueFbm(float3 position, float cells) {
  return ValueNoise(position, cells) * 0.5 + ValueNoise(position, cells * 2.0) * 0.25 + ValueNoise(position, cells * 4.0) * 0.125 + ValueNoise(position, cells * 8.0) * 0.0625;
}
float WorleyFbm(float3 position, float cells) {
  return Worley(position, cells) * 0.625 + Worley(position, cells * 2.0) * 0.25 + Worley(position, cells * 4.0) * 0.125;
}
float Remap(float value, float low, float high, float newLow, float newHigh) {
  return newLow + (value - low) / max(EPSILON, high - low) * (newHigh - newLow);
}
/// @brief The shape volume: where there is cloud at all, before anything erodes it.
///
/// Red is Perlin-Worley -- value noise remapped against an inverted Worley field -- which is the
/// combination that gives billows with wisps between them rather than either alone. The other three
/// are plain Worley at rising frequencies, summed by the sampler into the figure that decides how
/// much of the red survives.
[numthreads(4, 4, 4)]
void CSShapeNoise(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_noiseSize || id.y >= u_noiseSize || id.z >= u_noiseSize)
    return;
  float3 position = (float3(id) + 0.5) / (float)u_noiseSize;
  float perlin = ValueFbm(position, 4.0);
  float worley = WorleyFbm(position, 6.0);
  // Dilated by the Worley field rather than multiplied by it. Multiplying darkens everywhere and
  // leaves a field with no solid interior; remapping raises the floor where Worley says there is a
  // billow, which keeps the middle of a cloud opaque.
  float perlinWorley = Remap(perlin, worley - 1.0, 1.0, 0.0, 1.0);
  g_volume[id] = float4(saturate(perlinWorley), Worley(position, 6.0), Worley(position, 12.0), Worley(position, 24.0));
}
/// @brief The detail volume, which only ever cuts into an edge. Three Worley frequencies, no shape.
[numthreads(4, 4, 4)]
void CSDetailNoise(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_noiseSize || id.y >= u_noiseSize || id.z >= u_noiseSize)
    return;
  float3 position = (float3(id) + 0.5) / (float)u_noiseSize;
  g_volume[id] = float4(Worley(position, 4.0), Worley(position, 8.0), Worley(position, 16.0), 1.0);
}
// ---------------------------------------------------------------------------------------------
// The medium
// ---------------------------------------------------------------------------------------------
/// @brief Where in the layer an altitude sits, from zero at the base to one at the top.
float HeightFraction(float radius) {
  return saturate((radius - (u_bottomRadius + u_cloudBottom)) / max(EPSILON, u_cloudTop - u_cloudBottom));
}
/// @brief How much cloud a given height in the layer may hold at all.
///
/// A cumulus is narrow at the bottom, widest around a third of the way up and rounded off at the
/// top, and without this the layer comes out as a slab with flat faces -- which no amount of noise
/// disguises, because the flatness is in the silhouette rather than in the texture.
float HeightGradient(float fraction) {
  float base = saturate(Remap(fraction, 0.0, 0.12, 0.0, 1.0));
  float top = saturate(Remap(fraction, 0.55, 1.0, 1.0, 0.0));
  return base * top;
}
/// @brief Density of the cloud at a point, in the atmosphere's units of reciprocal radius.
///
/// @param position Atmosphere space, planet centred at the origin.
/// @param detail Whether to erode the edges, which the shadow march skips -- it is integrating an
///        optical depth rather than deciding a silhouette, and the erosion costs a second volume
///        fetch to move an answer it then averages away.
float CloudDensity(float3 position, bool detail) {
  float radius = length(position);
  float fraction = HeightFraction(radius);
  if (fraction <= 0.0 || fraction >= 1.0)
    return 0.0;
  // Into kilometres, so that `u_shapeScale` and `u_detailScale` mean what they say on the component
  // rather than meaning something per planet radius.
  //
  // Measured from the viewer rather than from the planet's centre, and that is precision rather
  // than taste. A position measured from the centre is six thousand kilometres from it, so dividing
  // by a detail scale of six hundred metres gives a coordinate around ten thousand that still has
  // to resolve a thousandth -- eight significant figures, where a float carries seven. What that
  // looks like is detail noise quantising into steps as the layer drifts. Measured from the viewer
  // the same coordinate is a few hundred at the horizon and a handful overhead.
  //
  // The origin is fixed for the frame and the field tiles, so subtracting it moves nothing: it is a
  // constant offset into a periodic function.
  float3 viewOrigin = float3(0.0, u_bottomRadius + max(0.0, u_viewAltitude), 0.0);
  float3 kilometres = (position - viewOrigin) * u_kilometres + u_windOffset;
  float3 shapeUV = kilometres / max(EPSILON, u_shapeScale);
  float4 shape = g_shapeNoise.SampleLevel(g_sampler, shapeUV, 0.0);
  float detailFbm = shape.g * 0.625 + shape.b * 0.25 + shape.a * 0.125;
  float base = saturate(Remap(shape.r, detailFbm - 1.0, 1.0, 0.0, 1.0));
  // Stretched back over the range it actually occupies before anything is thresholded against it.
  //
  // The dilation above never approaches zero: `Remap(x, w - 1, 1, 0, 1)` is `(x - w + 1) / (2 - w)`,
  // and with both fields centred near a half that lands around three quarters and varies by about a
  // seventh either side. Applied twice -- once building the volume and once reading it -- the field
  // that comes out sits in roughly [0.6, 0.9] and never once visits the bottom half of its own range.
  //
  // Threshold that directly and `coverage` is not a coverage: a half means total cover, and nothing
  // happens at all until it is nearly closed. The window below is where the field really lives, so
  // after this a coverage of a half puts cloud over about half the sky, which is what the number on
  // the component says it does.
  base = saturate(Remap(base, BASE_FLOOR, BASE_CEILING, 0.0, 1.0)) * HeightGradient(fraction);
  // Coverage subtracted here, at the end, rather than scaling the noise. That is what makes it read
  // as cover: the same field is thresholded lower, so clouds spread and join rather than each one
  // growing denser in place.
  float density = saturate(Remap(base, 1.0 - u_coverage, 1.0, 0.0, 1.0));
  if (density <= 0.0)
    return 0.0;
  if (detail && u_detailStrength > 0.0) {
    float3 detailUV = kilometres / max(EPSILON, u_detailScale);
    float3 fine = g_detailNoise.SampleLevel(g_sampler, detailUV, 0.0).rgb;
    float fineFbm = fine.r * 0.625 + fine.g * 0.25 + fine.b * 0.125;
    // Inverted towards the base and upright towards the top. The underside of a cloud is billowed
    // and the top is wispy, and one field can say both if it is turned over on the way up.
    float erosion = lerp(1.0 - fineFbm, fineFbm, saturate(fraction * 4.0));
    density = saturate(Remap(density, erosion * u_detailStrength, 1.0, 0.0, 1.0));
  }
  return density * u_density;
}
/// @brief Both intersections with a sphere about the origin, nearest first. False when the ray misses.
///
/// Both roots rather than the nearer one, because the cloud shell is two spheres and every span it
/// bounds is described by an entry into one and an exit from the other -- which of the four that is
/// depends on where the viewer stands, and having all of them to hand is what lets `CloudSpan` say
/// so plainly instead of rederiving a root it already threw away.
bool IntersectSphere(float3 origin, float3 direction, float radius, out float nearHit, out float farHit) {
  nearHit = 0.0;
  farHit = 0.0;
  float b = dot(origin, direction);
  // Factored rather than squared, for the reason `atmosphere.hlsl` sets out at length: forming two
  // large squares and subtracting them spends the mantissa before the subtraction that needs it.
  float distanceToCentre = length(origin);
  float c = (distanceToCentre - radius) * (distanceToCentre + radius);
  float discriminant = b * b - c;
  if (discriminant < 0.0)
    return false;
  float root = sqrt(discriminant);
  nearHit = -b - root;
  farHit = -b + root;
  return true;
}
/// @brief Where along a ray the cloud shell begins and ends, or false where it is never in it.
///
/// Three cases, and all three happen: `viewAltitude` is the scene's own altitude, so a scene may sit
/// under the layer, inside it, or above it, and a chess board at two kilometres under a base at one
/// and a half is the middle one. Getting this wrong does not fail loudly -- it produces a sky with
/// cloud in the wrong half of it.
///
/// Clipped against the planet as well, so a ray that meets the ground carries no cloud beyond the
/// point where it did.
bool CloudSpan(float3 origin, float3 direction, out float near, out float far) {
  near = 0.0;
  far = 0.0;
  float radius = length(origin);
  float innerRadius = u_bottomRadius + u_cloudBottom;
  float outerRadius = u_bottomRadius + u_cloudTop;
  float outerNear, outerFar;
  if (!IntersectSphere(origin, direction, outerRadius, outerNear, outerFar) || outerFar <= 0.0)
    return false;
  float innerNear, innerFar;
  bool hitsInner = IntersectSphere(origin, direction, innerRadius, innerNear, innerFar);
  if (radius < innerRadius) {
    // Under the layer. The ray starts inside the inner shell, so it leaves it at the far root, and
    // that is where the cloud starts.
    if (!hitsInner)
      return false;
    near = innerFar;
    far = outerFar;
  } else if (radius > outerRadius) {
    // Over it. Enters at the outer shell, and leaves either at the layer's underside or, for a ray
    // that passes over without meeting it, at the far side of the outer shell.
    near = max(0.0, outerNear);
    far = hitsInner && innerNear > near ? innerNear : outerFar;
  } else {
    // In it, which is where a scene between the cloud base and the cloud top stands.
    near = 0.0;
    far = hitsInner && innerNear > 0.0 ? innerNear : outerFar;
  }
  float groundNear, groundFar;
  if (IntersectSphere(origin, direction, u_bottomRadius, groundNear, groundFar) && groundNear > 0.0)
    far = min(far, groundNear);
  return far > near;
}
float HenyeyGreenstein(float cosTheta, float g) {
  float g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}
/// @brief Forward and backward lobes mixed, which one lobe cannot do.
///
/// A cloud with the sun behind it has a bright rim, and a cloud with the sun behind the viewer is
/// bright across its face. The first is the forward lobe and the second is the backward one, and a
/// single Henyey-Greenstein term has to choose.
float CloudPhase(float cosTheta) {
  return lerp(HenyeyGreenstein(cosTheta, u_anisotropy), HenyeyGreenstein(cosTheta, -u_anisotropy * 0.5), u_backscatter);
}
float2 TransmittanceUV(float radius, float cosZenith) {
  float horizon = sqrt(max(0.0, (u_topRadius - u_bottomRadius) * (u_topRadius + u_bottomRadius)));
  float ground = sqrt(max(0.0, (radius - u_bottomRadius) * (radius + u_bottomRadius)));
  float discriminant = (u_topRadius - radius) * (u_topRadius + radius) + radius * cosZenith * radius * cosZenith;
  float reach = max(0.0, -radius * cosZenith + sqrt(max(0.0, discriminant)));
  float nearest = u_topRadius - radius;
  float furthest = ground + horizon;
  float x = (reach - nearest) / max(EPSILON, furthest - nearest);
  float y = ground / max(EPSILON, horizon);
  // The same half-texel inset `atmosphere.hlsl` addresses this table with. Reading it any other way
  // puts a seam along the horizon, where a ray is nearly tangent and transmittance changes fastest.
  float2 inset = 0.5 / float2((float)u_width, (float)u_height);
  return inset + saturate(float2(x, y)) * (1.0 - 2.0 * inset);
}
/// @brief Sunlight arriving at a point inside the layer, with the air above it already accounted.
float3 SunRadiance(float3 position) {
  float radius = max(u_bottomRadius, length(position));
  float cosZenith = dot(position / max(EPSILON, radius), u_sunDirection);
  float cosHorizon = -sqrt(max(0.0, 1.0 - (u_bottomRadius * u_bottomRadius) / (radius * radius)));
  if (cosZenith < cosHorizon)
    return float3(0.0, 0.0, 0.0);
  return g_transmittance.SampleLevel(g_clampSampler, TransmittanceUV(radius, cosZenith), 0.0).rgb * u_sunIntensity;
}
/// @brief Optical depth from a point to the top of the layer along the sun, as a transmittance.
///
/// Six steps by default and distributed over the span rather than at a fixed length, because what
/// it is integrating is smooth and because the several-orders term covers what a short march
/// misses. The steps grow as they go, which spends them where the answer is still changing.
float CloudSunTransmittance(float3 position, uint steps) {
  float near, far;
  if (!CloudSpan(position, u_sunDirection, near, far))
    return 1.0;
  float span = min(far - near, (u_cloudTop - u_cloudBottom) * 2.0);
  if (span <= 0.0 || steps == 0)
    return 1.0;
  float opticalDepth = 0.0;
  float travelled = 0.0;
  float stepLength = span / (float)steps;
  for (uint index = 0; index < steps; ++index) {
    // Each step longer than the last, which puts the resolution near the point being shaded where
    // the cloud it is standing in matters most.
    float stepSpan = stepLength * (0.5 + (float)index * 0.35);
    travelled += stepSpan;
    opticalDepth += CloudDensity(position + u_sunDirection * travelled, false) * stepSpan;
  }
  return exp(-opticalDepth);
}
/// @brief Interleaved gradient noise, which is what dithers the march's starting offset.
///
/// A hash on the pixel rather than a texture, and the frame folded into it so that what a short
/// march costs is a grain that changes every frame rather than a fixed pattern the eye locks onto.
float Dither(float2 pixel, uint frame) {
  float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
  return frac(magic.z * frac(dot(pixel + (float)(frame & 63u) * 5.588238, magic.xy)));
}
// ---------------------------------------------------------------------------------------------
// Passes
// ---------------------------------------------------------------------------------------------
/// @brief What the sun has left after the cloud layer, seen from the scene, as a single texel.
///
/// One value on the GPU rather than a number worked out on the processor, which is the arrangement
/// `CSSunTransmittance` in `atmosphere.hlsl` already uses and for the same reason: the answer is an
/// integral through a medium that only a shader describes, and computing it in C++ would be a
/// second copy of the cloud to keep in step with this one.
///
/// This is the whole of what the clouds do to the light below them. `volumetric.hlsl` multiplies
/// the sun's contribution to every shaft by it, so a cloud drifting across the sun takes the beams
/// with it. What it does not reach is surface shading: the scene is shaded before this pass runs
/// and reads nothing from here, so the ground under an overcast sky is lit as though it were clear.
/// Closing that means the scene pass sampling a cloud shadow map, which is a larger change than
/// this one and is not made here.
[numthreads(1, 1, 1)]
void CSSunTransmittance(uint3 id : SV_DispatchThreadID) {
  if (id.x > 0 || id.y > 0)
    return;
  float3 origin = float3(0.0, u_bottomRadius + max(0.0, u_viewAltitude), 0.0);
  float transmittance = CloudSunTransmittance(origin, max(4u, u_lightSteps * 2u));
  // Interpolated towards one by the strength rather than multiplied by it, so that turning the
  // strength down lightens the shadow instead of scaling a transmittance towards black.
  g_lut[uint2(0, 0)] = float4(lerp(1.0, transmittance, saturate(u_shadowStrength)).xxx, 1.0);
}
struct PSInput {
  float4 position : SV_POSITION;
  float2 ndc : TEXCOORD0;
  float2 uv : TEXCOORD1;
};
PSInput VSMain(uint id : SV_VertexID) {
  PSInput output;
  float2 corner = float2((id << 1) & 2, id & 2);
  output.position = float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  output.ndc = output.position.xy;
  output.uv = corner;
  return output;
}
/// @brief One half-resolution pixel of cloud, premultiplied.
///
/// Premultiplied because that is what makes an empty sky free: the colour is already scaled by how
/// much of the pixel the cloud covers and the alpha is that coverage, so the pass that reads this
/// composites with `behind * (1 - a) + rgb` and a zeroed buffer is a clear sky rather than a black
/// one. It is also what lets the half-resolution upsample interpolate: coverage and premultiplied
/// colour both interpolate linearly, where colour and coverage kept separate do not.
float4 PSMain(PSInput input) : SV_TARGET {
  // The farthest of four full-resolution depths around this pixel. Conservative in the direction
  // that matters: a half-resolution pixel near a silhouette is marched rather than skipped, so the
  // sky beside the silhouette gets its cloud. The taps reach a texel and a half out rather than
  // half a texel so that they also cover what a bilinear read of this buffer will reach for --
  // skipping a texel that the pass downstream then blends from is what leaves a dark seam tracing
  // every object in the scene.
  float2 texel = 1.0 / float2((float)u_width, (float)u_height);
  float depth = 0.0;
  depth = max(depth, g_depth.SampleLevel(g_pointSampler, input.uv + float2(-1.5, -1.5) * texel, 0.0).r);
  depth = max(depth, g_depth.SampleLevel(g_pointSampler, input.uv + float2(1.5, -1.5) * texel, 0.0).r);
  depth = max(depth, g_depth.SampleLevel(g_pointSampler, input.uv + float2(-1.5, 1.5) * texel, 0.0).r);
  depth = max(depth, g_depth.SampleLevel(g_pointSampler, input.uv + float2(1.5, 1.5) * texel, 0.0).r);
  // Anything short of the far plane is geometry, and the cloud layer is kilometres beyond anything
  // a scene contains, so there is nothing to draw here.
  if (depth < 1.0)
    return float4(0.0, 0.0, 0.0, 0.0);
  float4 unprojected = mul(u_inverseViewProjection, float4(input.ndc, 1.0, 1.0));
  float3 direction = normalize(unprojected.xyz / unprojected.w);
  float3 origin = float3(0.0, u_bottomRadius + max(0.0, u_viewAltitude), 0.0);
  float near, far;
  if (!CloudSpan(origin, direction, near, far))
    return float4(0.0, 0.0, 0.0, 0.0);
  uint steps = max(8u, u_steps);
  float span = far - near;
  // Capped against the layer's own thickness rather than simply divided by the step count, and this
  // is what makes a cloud a cloud rather than a wall. A ray towards the horizon crosses the shell
  // for hundreds of kilometres, so dividing that span by sixty-four gives steps kilometres long --
  // and one step of a kilometre through a medium this thick saturates on its first sample, so every
  // such ray comes back fully opaque and the whole horizon reads as a flat white band.
  //
  // A sixth of the thickness resolves the vertical structure, which is the structure there is. What
  // it costs is the far end of a grazing ray, which goes unmarched -- and is behind ten kilometres
  // of cloud that the near end already made opaque, so there was nothing there to see.
  float thickness = max(EPSILON, u_cloudTop - u_cloudBottom);
  float stepLength = min(span / (float)steps, thickness / 6.0);
  float offset = Dither(input.position.xy, u_frame);
  float cosSun = dot(direction, u_sunDirection);
  // Read once outside the loop. It is a blurred cubemap fetch standing for light arriving from the
  // whole sky, and the sky does not change appreciably across the few kilometres one ray crosses.
  float3 ambientSky = g_sky.SampleLevel(g_clampSampler, float3(0.0, 1.0, 0.0), AMBIENT_MIP).rgb * u_ambient;
  float3 groundSky = g_sky.SampleLevel(g_clampSampler, float3(0.0, -1.0, 0.0), AMBIENT_MIP).rgb * u_ambient;
  float3 scattering = float3(0.0, 0.0, 0.0);
  float transmittance = 1.0;
  for (uint index = 0; index < steps; ++index) {
    if (transmittance < 0.01)
      break;
    float3 position = origin + direction * (near + stepLength * ((float)index + offset));
    float density = CloudDensity(position, true);
    if (density <= 0.0)
      continue;
    float stepTransmittance = exp(-density * stepLength);
    float3 sunlight = SunRadiance(position);
    float sunTransmittance = CloudSunTransmittance(position, max(1u, u_lightSteps));
    // Beer's law says a thin edge seen against the sun is bright, and a cloud's edge is not: light
    // entering there has to scatter several times before it leaves, and most of it does not. The
    // powder term is the correction, and it is why the underside of a cumulus reads as bruised
    // rather than as grey.
    float powder = 1.0 - exp(-density * stepLength * 4.0);
    float powderTerm = lerp(1.0, powder, u_powder);
    // Sky above and ground below, weighted by where in the layer this is. The top of a cloud is
    // filled by the sky and its base by whatever the ground is bouncing back, which is most of why
    // a cloud base is darker than its top before any shadow is taken into account.
    float fraction = HeightFraction(length(position));
    float3 ambient = lerp(groundSky, ambientSky, fraction);
    float3 gathered = float3(0.0, 0.0, 0.0);
    float octaveExtinction = 1.0;
    float octaveScattering = 1.0;
    float octaveEccentricity = 1.0;
    for (uint octave = 0; octave < SCATTERING_OCTAVES; ++octave) {
      float octavePhase = CloudPhase(cosSun * octaveEccentricity);
      gathered += sunlight * exp(-(1.0 - sunTransmittance) * octaveExtinction * 4.0) * sunTransmittance * octavePhase * octaveScattering;
      octaveExtinction *= OCTAVE_EXTINCTION;
      octaveScattering *= OCTAVE_SCATTERING;
      octaveEccentricity *= OCTAVE_ECCENTRICITY;
    }
    gathered = gathered * powderTerm + ambient;
    // Integrated across the step rather than sampled at its middle, which is what keeps a thick
    // cloud from depending on the step count -- the case where extinction is highest and a midpoint
    // sample is furthest from the average it stands for.
    //
    // The density that would weight this cancels against the extinction that divides it, because a
    // cloud droplet scatters very nearly everything it removes: the albedo is one to three decimal
    // places, so `scattering / extinction` is one and what is left is the fraction of the step's
    // light that was scattered at all. Writing the cancellation out rather than leaving both
    // factors in is the difference between this reading as an identity and reading as a bug.
    scattering += transmittance * gathered * (1.0 - stepTransmittance);
    transmittance *= stepTransmittance;
  }
  float coverage = saturate(1.0 - transmittance);
  return float4(scattering * u_scatteringScale, coverage);
}
