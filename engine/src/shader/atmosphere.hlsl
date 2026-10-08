// Rayleigh, Mie and ozone scattering through a spherical atmosphere, precomputed into the same
// cubemap an equirectangular skybox would have filled.
//
// The medium is Bruneton and Neyret's: air that scatters by Rayleigh with an 8km scale height, an
// aerosol layer that scatters forward by Mie with a 1.2km one, and an ozone layer that absorbs
// without scattering and is what makes a sunset red rather than merely dim. The parameterisation of
// the transmittance table is Bruneton's own, from the 2017 reference implementation, because
// getting that mapping wrong is what puts a visible seam along the horizon.
//
// What is not Bruneton's is the shape of the precompute. His stores single and multiple scattering
// in a four-dimensional table and reads the sky out of it per pixel per frame. Here the sky is
// evaluated into a cubemap that is rebuilt only when the sun moves -- the engine already keeps that
// cubemap, mips it, projects it to harmonics and prefilters it for reflections -- so the expensive
// table would be a cache in front of a cache. The two tables that remain are the ones that earn
// their keep inside a single march: transmittance, which the inner loop would otherwise integrate
// per step, and Hillaire's multiple-scattering estimate, which nothing else here could supply.
//
// Everything is in kilometres and the planet is centred at the origin with the viewer on +Y. None
// of that has to agree with the scene's units: the sky is a background at infinity, and
// `u_viewAltitude` says how high the scene is meant to be in the only units the atmosphere has.
static const float PI = 3.14159265359;
static const float ISOTROPIC_PHASE = 0.079577471; // 1 / 4pi
static const float GOLDEN_ANGLE = 2.39996323;
static const uint MULTISCATTER_DIRECTIONS = 64;
static const uint MULTISCATTER_STEPS = 20;
static const float EPSILON = 1.0e-6;
cbuffer AtmosphereConstants : register(b0) {
  float u_bottomRadius;
  float u_topRadius;
  float u_rayleighScaleHeight;
  float u_mieScaleHeight;
  float3 u_rayleighScattering;
  float u_mieScattering;
  float u_mieExtinction;
  float u_mieAnisotropy;
  float u_groundAlbedo;
  float u_multiscatterStrength;
  float3 u_ozoneAbsorption;
  float u_ozoneCenter;
  float3 u_sunDirection;
  float u_ozoneWidth;
  float u_sunIntensity;
  float u_viewAltitude;
  uint u_marchSteps;
  uint u_width;
  uint u_height;
  uint u_size;
  uint u_padding0;
  uint u_padding1;
};
Texture2D g_transmittance : register(t0);
Texture2D g_multiscatter : register(t1);
RWTexture2D<float4> g_lut : register(u0);
RWTexture2DArray<float4> g_cubemap : register(u1);
SamplerState g_sampler : register(s0);
// The half-texel inset Bruneton's tables are addressed with. Without it the first and last texel of
// each axis stands for a value just inside the range it is meant to bound, and the error lands
// exactly at the horizon -- where a ray is nearly tangent and the transmittance changes fastest.
float UnitFromTexel(float texel, float extent) {
  return (texel / extent - 0.5 / extent) / (1.0 - 1.0 / extent);
}
float TexelFromUnit(float unit, float extent) {
  return 0.5 / extent + saturate(unit) * (1.0 - 1.0 / extent);
}
/// @brief `a * a - b * b`, factored so that it survives single precision at planetary radii.
///
/// The one piece of arithmetic in this file that cannot be written the obvious way. Both arguments
/// here are radii around 6360, each square is therefore around 4e7 and needs twenty-six bits of
/// mantissa where a float has twenty-four -- so subtracting two of them returns noise whenever the
/// true answer is small, which is precisely the case that matters: a point on the ground, where the
/// radius equals the planet's and the difference should be zero.
///
/// `(a - b) * (a + b)` is the same number reached without ever forming either square. The
/// difference between the two is not subtle. Written the obvious way, the lower half of the sky --
/// every direction that meets the ground -- comes out as structured speckle instead of ground.
float DifferenceOfSquares(float a, float b) {
  return (a - b) * (a + b);
}
/// Distance to the near intersection with a sphere about the origin, or -1 when the ray misses it.
float IntersectSphere(float3 origin, float3 direction, float radius) {
  float b = dot(origin, direction);
  float c = DifferenceOfSquares(length(origin), radius);
  float discriminant = b * b - c;
  if (discriminant < 0.0)
    return -1.0;
  float root = sqrt(discriminant);
  float nearHit = -b - root;
  float farHit = -b + root;
  if (farHit < 0.0)
    return -1.0;
  return nearHit < 0.0 ? farHit : nearHit;
}
/// How far a ray may travel before it leaves the atmosphere or meets the ground.
float RaySpan(float3 origin, float3 direction) {
  float top = IntersectSphere(origin, direction, u_topRadius);
  float ground = IntersectSphere(origin, direction, u_bottomRadius);
  if (top < 0.0)
    return 0.0;
  return ground > 0.0 ? min(top, ground) : top;
}
/// Scattering and extinction coefficients at an altitude, in reciprocal kilometres.
///
/// Ozone is a tent rather than an exponential: it is a layer at an altitude rather than a gas that
/// thins with height, and modelling it as the latter leaves a sunset orange instead of red. It
/// absorbs and does not scatter, so it appears in the extinction and in neither scattering term.
void SampleMedium(float altitude, out float3 rayleigh, out float mie, out float3 extinction) {
  float clamped = max(0.0, altitude);
  float rayleighDensity = exp(-clamped / max(EPSILON, u_rayleighScaleHeight));
  float mieDensity = exp(-clamped / max(EPSILON, u_mieScaleHeight));
  float ozoneDensity = max(0.0, 1.0 - abs(clamped - u_ozoneCenter) / max(EPSILON, u_ozoneWidth));
  rayleigh = u_rayleighScattering * rayleighDensity;
  mie = u_mieScattering * mieDensity;
  extinction = rayleigh + u_mieExtinction * mieDensity + u_ozoneAbsorption * ozoneDensity;
}
float RayleighPhase(float cosTheta) {
  return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}
/// Cornette-Shanks, which is the Henyey-Greenstein lobe corrected to integrate to one.
float MiePhase(float cosTheta, float g) {
  float g2 = g * g;
  float numerator = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
  float denominator = 8.0 * PI * (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5);
  return numerator / max(EPSILON, denominator);
}
/// Bruneton's mapping from a radius and a view zenith cosine onto the transmittance table.
float2 TransmittanceUV(float radius, float cosZenith) {
  float horizon = sqrt(max(0.0, DifferenceOfSquares(u_topRadius, u_bottomRadius)));
  float ground = sqrt(max(0.0, DifferenceOfSquares(radius, u_bottomRadius)));
  // `topRadius^2 - radius^2 + (radius * cosZenith)^2`, which is the same discriminant written so
  // that the only subtraction of two large squares is the one the helper above takes care of.
  float discriminant = DifferenceOfSquares(u_topRadius, radius) + radius * cosZenith * radius * cosZenith;
  float reach = max(0.0, -radius * cosZenith + sqrt(max(0.0, discriminant)));
  float nearest = u_topRadius - radius;
  float furthest = ground + horizon;
  float x = (reach - nearest) / max(EPSILON, furthest - nearest);
  float y = ground / max(EPSILON, horizon);
  return float2(TexelFromUnit(x, (float)u_width), TexelFromUnit(y, (float)u_height));
}
/// The inverse, which is what the table's own threads need to know what they stand for.
void RadiusCosineFromUV(float2 uv, out float radius, out float cosZenith) {
  float x = UnitFromTexel(uv.x * (float)u_width, (float)u_width);
  float y = UnitFromTexel(uv.y * (float)u_height, (float)u_height);
  float horizon = sqrt(max(0.0, DifferenceOfSquares(u_topRadius, u_bottomRadius)));
  float ground = horizon * saturate(y);
  radius = sqrt(ground * ground + u_bottomRadius * u_bottomRadius);
  float nearest = u_topRadius - radius;
  float furthest = ground + horizon;
  float reach = nearest + saturate(x) * (furthest - nearest);
  cosZenith = reach < EPSILON ? 1.0 : (DifferenceOfSquares(horizon, ground) - reach * reach) / (2.0 * radius * reach);
  cosZenith = clamp(cosZenith, -1.0, 1.0);
}
/// @brief Whether the planet stands between a point and the sun.
///
/// Asked as an angle rather than as a ray-sphere intersection, and this matters more than it looks.
/// The points this is asked about most are exactly on the sphere -- every ground hit is one -- and
/// an intersection test against the surface a point is already sitting on is decided by the sign of
/// that point's own rounding error. Half the directions come back lit and half shadowed, in bands,
/// because the error falls the same way for neighbouring rays: the ground rendered as a set of
/// concentric rings centred on the nadir.
///
/// The angle has no such failure. A point at radius `r` can see the sun exactly while the sun sits
/// above its local horizon, and that horizon is an arccosine of `bottom / r` -- zero at the ground,
/// tending to straight down far above it. At the ground it reduces to "is the sun above me", which
/// is exact.
bool PlanetBlocksSun(float3 position) {
  float radius = max(u_bottomRadius, length(position));
  float cosZenith = dot(position / max(EPSILON, radius), u_sunDirection);
  float cosHorizon = -sqrt(max(0.0, 1.0 - (u_bottomRadius * u_bottomRadius) / (radius * radius)));
  return cosZenith < cosHorizon;
}
float3 SampleTransmittance(float radius, float cosZenith) {
  return g_transmittance.SampleLevel(g_sampler, TransmittanceUV(radius, cosZenith), 0).rgb;
}
/// Transmittance towards the sun from a point, and nothing at all where the planet is in the way.
float3 SunTransmittance(float3 position) {
  if (PlanetBlocksSun(position))
    return float3(0.0, 0.0, 0.0);
  float radius = length(position);
  return SampleTransmittance(radius, dot(position / max(EPSILON, radius), u_sunDirection));
}
float2 MultiscatterUV(float radius, float cosSunZenith) {
  float x = saturate(0.5 + 0.5 * cosSunZenith);
  float y = saturate((radius - u_bottomRadius) / max(EPSILON, u_topRadius - u_bottomRadius));
  return float2(x, y);
}
/// @brief One texel of the transmittance table: what survives a trip from here out to space.
[numthreads(8, 8, 1)]
void CSTransmittance(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_width || id.y >= u_height)
    return;
  float2 uv = (float2(id.xy) + 0.5) / float2((float)u_width, (float)u_height);
  float radius, cosZenith;
  RadiusCosineFromUV(uv, radius, cosZenith);
  float3 origin = float3(0.0, radius, 0.0);
  // The zenith cosine is measured against the local up, which here is +Y, so the ray leans off it
  // by the matching sine. Which way it leans around Y does not matter: the medium is spherically
  // symmetric, so every azimuth gives the same answer.
  float3 direction = float3(sqrt(max(0.0, 1.0 - cosZenith * cosZenith)), cosZenith, 0.0);
  float span = RaySpan(origin, direction);
  uint steps = max(16u, u_marchSteps);
  float stepLength = span / (float)steps;
  float3 opticalDepth = float3(0.0, 0.0, 0.0);
  for (uint index = 0; index < steps; ++index) {
    float3 position = origin + direction * (stepLength * ((float)index + 0.5));
    float3 rayleigh, extinction;
    float mie;
    SampleMedium(length(position) - u_bottomRadius, rayleigh, mie, extinction);
    opticalDepth += extinction * stepLength;
  }
  g_lut[id.xy] = float4(exp(-opticalDepth), 1.0);
}
/// @brief The sun's own colour, seen from the scene, as a single texel.
///
/// One value, written on the GPU rather than worked out on the processor, and that is the whole
/// point of it. The background pass needs to know how much of the sun survives the air between the
/// viewer and space -- which is what turns the disc orange as it sets -- and that answer is an
/// integral through the same medium everything else here marches. Computing it a second time in C++
/// would be a second copy of the atmosphere to keep in step with this one.
///
/// Zero when the planet is in the way, so a sun below the horizon leaves no disc behind.
[numthreads(1, 1, 1)]
void CSSunTransmittance(uint3 id : SV_DispatchThreadID) {
  if (id.x > 0 || id.y > 0)
    return;
  float3 origin = float3(0.0, u_bottomRadius + max(0.0, u_viewAltitude), 0.0);
  g_lut[uint2(0, 0)] = float4(SunTransmittance(origin), 1.0);
}
/// @brief One texel of Hillaire's multiple-scattering estimate.
///
/// Light that reached a point after bouncing more than once has lost its direction, so it is
/// gathered with an isotropic phase and stored per altitude and sun angle rather than per view
/// direction. The series is then summed in closed form: `transfer` is the fraction of light that
/// scatters again on its way out, so the total over infinitely many orders is the second order
/// divided by `1 - transfer`. Leaving this out is what makes an unaided single-scattering sky too
/// dark towards the horizon, and it drops the blue hour after sunset entirely.
[numthreads(8, 8, 1)]
void CSMultiscatter(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_width || id.y >= u_height)
    return;
  float cosSunZenith = clamp(2.0 * ((float)id.x + 0.5) / (float)u_width - 1.0, -1.0, 1.0);
  float altitudeFraction = ((float)id.y + 0.5) / (float)u_height;
  float radius = lerp(u_bottomRadius, u_topRadius, max(altitudeFraction, 1.0e-3));
  float3 origin = float3(0.0, radius, 0.0);
  float3 sunDirection = float3(sqrt(max(0.0, 1.0 - cosSunZenith * cosSunZenith)), cosSunZenith, 0.0);
  float3 secondOrder = float3(0.0, 0.0, 0.0);
  float3 transfer = float3(0.0, 0.0, 0.0);
  for (uint slice = 0; slice < MULTISCATTER_DIRECTIONS; ++slice) {
    // A Fibonacci spiral over the sphere, which needs no table and has no clumping to speak of at
    // this many samples. Every direction carries the same weight, so the isotropic phase and the
    // sphere's solid angle cancel and the mean over directions is the integral over them.
    float offset = ((float)slice + 0.5) / (float)MULTISCATTER_DIRECTIONS;
    float cosTheta = 1.0 - 2.0 * offset;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = GOLDEN_ANGLE * (float)slice;
    float3 direction = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
    float span = RaySpan(origin, direction);
    if (span <= 0.0)
      continue;
    float stepLength = span / (float)MULTISCATTER_STEPS;
    float3 throughput = float3(1.0, 1.0, 1.0);
    for (uint index = 0; index < MULTISCATTER_STEPS; ++index) {
      float3 position = origin + direction * (stepLength * ((float)index + 0.5));
      float positionRadius = length(position);
      float3 rayleigh, extinction;
      float mie;
      SampleMedium(positionRadius - u_bottomRadius, rayleigh, mie, extinction);
      float3 scattering = rayleigh + mie;
      float3 stepTransmittance = exp(-extinction * stepLength);
      // Integrated across the step rather than sampled at its middle, which is what keeps the
      // estimate from depending on the step count near the ground, where extinction is highest.
      float3 integrated = (1.0 - stepTransmittance) / max(EPSILON, extinction);
      // Inlined rather than `SunTransmittance`, because this table is built for a sun direction of
      // its own rather than the one in the constants.
      float3 sunTransmittance = float3(0.0, 0.0, 0.0);
      float cosSun = dot(position / max(EPSILON, positionRadius), sunDirection);
      float cosHorizon = -sqrt(max(0.0, 1.0 - (u_bottomRadius * u_bottomRadius) / (positionRadius * positionRadius)));
      if (cosSun >= cosHorizon)
        sunTransmittance = SampleTransmittance(positionRadius, cosSun);
      // The uniform phase belongs here, at the point where the sunlight scatters, and in both
      // terms. It is easy to leave out: the outer integral over directions carries a 4pi of
      // solid angle that cancels the gathering phase against it, so the mean over samples needs
      // no factor of its own -- but the scattering event inside the ray is a second one, and it
      // does. Without it the table comes out 4pi times too large, which reaches the sky as a
      // view-independent term several times the size of the single scattering it is meant to
      // supplement: the gradient flattens out and the whole sky goes to one bright colour.
      secondOrder += throughput * scattering * sunTransmittance * integrated * ISOTROPIC_PHASE;
      transfer += throughput * scattering * integrated * ISOTROPIC_PHASE;
      throughput *= stepTransmittance;
    }
  }
  float count = (float)MULTISCATTER_DIRECTIONS;
  secondOrder /= count;
  transfer /= count;
  g_lut[id.xy] = float4(secondOrder / max(EPSILON, 1.0 - min(transfer, float3(0.999, 0.999, 0.999))), 1.0);
}
/// Face and axis conventions copied from `ibl.hlsl` deliberately: every consumer downstream -- the
/// mip chain, the harmonics, the prefiltered reflections and the background draw -- already agrees
/// with that one, and a second convention over the same cubemap is a sky that no longer matches the
/// light coming off the scene.
float3 CubeDirectionFromCoord(uint3 coord, uint size) {
  float s = 2.0 * ((float)coord.x + 0.5) / (float)size - 1.0;
  float t = 2.0 * ((float)coord.y + 0.5) / (float)size - 1.0;
  float3 direction;
  switch (coord.z) {
  case 0:
    direction = float3(1.0, -t, -s);
    break;
  case 1:
    direction = float3(-1.0, -t, s);
    break;
  case 2:
    direction = float3(s, 1.0, t);
    break;
  case 3:
    direction = float3(s, -1.0, -t);
    break;
  case 4:
    direction = float3(s, -t, 1.0);
    break;
  default:
    direction = float3(-s, -t, -1.0);
    break;
  }
  return normalize(direction);
}
/// @brief One texel of the sky, written where an equirectangular skybox would have been unpacked.
[numthreads(8, 8, 1)]
void CSSkyToCubemap(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size || id.z >= 6)
    return;
  float3 direction = CubeDirectionFromCoord(id, u_size);
  float3 origin = float3(0.0, u_bottomRadius + max(0.0, u_viewAltitude), 0.0);
  float span = RaySpan(origin, direction);
  float3 luminance = float3(0.0, 0.0, 0.0);
  float3 throughput = float3(1.0, 1.0, 1.0);
  if (span > 0.0) {
    float cosSun = dot(direction, u_sunDirection);
    float rayleighPhase = RayleighPhase(cosSun);
    float miePhase = MiePhase(cosSun, u_mieAnisotropy);
    uint steps = max(8u, u_marchSteps);
    float stepLength = span / (float)steps;
    for (uint index = 0; index < steps; ++index) {
      float3 position = origin + direction * (stepLength * ((float)index + 0.5));
      float positionRadius = length(position);
      float3 rayleigh, extinction;
      float mie;
      SampleMedium(positionRadius - u_bottomRadius, rayleigh, mie, extinction);
      float3 stepTransmittance = exp(-extinction * stepLength);
      float3 integrated = (1.0 - stepTransmittance) / max(EPSILON, extinction);
      float3 up = position / max(EPSILON, positionRadius);
      float cosSunZenith = dot(up, u_sunDirection);
      float3 sunTransmittance = PlanetBlocksSun(position) ? float3(0.0, 0.0, 0.0) : SampleTransmittance(positionRadius, cosSunZenith);
      float3 single = (rayleigh * rayleighPhase + mie * miePhase) * sunTransmittance;
      float3 multiple = (rayleigh + mie) * g_multiscatter.SampleLevel(g_sampler, MultiscatterUV(positionRadius, cosSunZenith), 0).rgb * u_multiscatterStrength;
      luminance += throughput * (single + multiple) * integrated;
      throughput *= stepTransmittance;
    }
    // What the ground adds when the ray meets it, which is most of the lower half of the cubemap
    // and therefore most of what the irradiance and the reflections are gathered from. A Lambertian
    // bounce of the sunlight reaching that point, so a scene over bright sand is lit from below and
    // one over dark water is not.
    if (IntersectSphere(origin, direction, u_bottomRadius) > 0.0) {
      float3 ground = origin + direction * span;
      float3 normal = normalize(ground);
      luminance += throughput * SunTransmittance(ground) * max(0.0, dot(normal, u_sunDirection)) * u_groundAlbedo / PI;
    }
  }
  g_cubemap[id] = float4(luminance * u_sunIntensity, 1.0);
}
