// The air between the camera and the scene, and what the scene's lights scatter out of it.
//
// A beam of light is invisible from the side. What is seen when a shaft of sunlight comes through a
// window is not the light but the dust it is passing through, and every renderer that draws one is
// drawing the dust. This marches the camera ray through a height fog, asks each light's shadow map
// at every step whether that light reaches this point, and adds what would scatter towards the
// camera if it does -- which is the whole of the effect and why it costs so little: the expensive
// question was answered by the shadow pass before the frame started shading anything.
//
// It also composites, and that is deliberate rather than convenient. The pass is handed the
// finished scene colour, the cloud buffer, and the depth that says which is which, and everything
// between the camera and whatever each pixel is looking at has to be applied in one place and in
// one order -- cloud behind, then haze in front. Writing the scattering to a fourth full-screen
// target for somebody else to combine would be a target for a term with exactly one consumer, and
// would put the ordering somewhere other than where the reasoning for it is.
//
// Two media, and they do not overlap. `u_density` and the three fields with it describe a scene's
// own air: a room with dust in it, measured in the scene's units, which is what a sealed box has
// and what a shaft needs. `u_aerialPerspective` applies the atmosphere's air instead, in kilometres
// of Rayleigh and Mie read off the same figures the sky was marched from, which is what makes a far
// surface go blue and is the only reason distance reads as distance outdoors. A scene may have
// either, both, or neither.
// Named from the definitions every shader here is compiled with, exactly as `scene.hlsl` does it.
// The constant buffer below has to match that pass's byte for byte, so the array lengths have to
// come from the same place rather than being written down again. See `shader_definitions.hpp`.
static const uint MAX_POINT_LIGHTS = KUKI_MAX_POINT_LIGHTS;
static const uint MAX_SPOT_LIGHTS = KUKI_MAX_SPOT_LIGHTS;
static const float PI = 3.14159265359;
static const float EPSILON = 1.0e-6;
/// @brief Depth bias for a shadow lookup taken in mid air, in the shadow map's own depth units.
///
/// Far smaller than the shading pass uses, and it is worth saying why rather than leaving it looking
/// like a number somebody tuned down. The bias there is fighting surface acne: a shaded point is
/// exactly on the geometry the shadow map recorded, so the comparison is between a value and itself
/// and the slightest disagreement decides it wrongly across a whole surface. A point in the air is
/// not on anything. Nothing here is self-shadowing, so the bias only has to cover the map's own
/// quantisation, and a large one would visibly detach every shaft from the edge that cast it.
static const float VOLUMETRIC_SHADOW_BIAS = 0.0008;
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
cbuffer VolumetricConstants : register(b0) {
  float4x4 u_inverseViewProjection;
  float3 u_cameraPosition;
  float u_maxDistance;
  float3 u_albedo;
  float u_anisotropy;
  float3 u_ambient;
  float u_lightScale;
  float u_density;
  float u_baseHeight;
  float u_heightFalloff;
  float u_aerialPerspective;
  float3 u_rayleighScattering;
  float u_mieScattering;
  float3 u_extinction;
  float u_mieAnisotropy;
  uint u_steps;
  uint u_frame;
  uint u_hasAtmosphere;
  uint u_hasClouds;
  float u_kilometresPerUnit;
  float u_sunIntensity;
  float u_padding0;
  float u_padding1;
};
// The shading pass's own frame constants, redeclared rather than reduced to what is read here --
// the same arrangement `probe_trace.hlsl` uses, and for the same reason. A cbuffer packs by
// declaration order, so a pass that skipped a vector another pass declares would shift everything
// past it and read somebody else's numbers. The unread ones are here to hold the layout.
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
};
Texture2D g_scene : register(t0);
Texture2D g_depth : register(t1);
Texture2D g_shadowMap : register(t2);
Texture2DArray g_spotShadowMap : register(t3);
Texture2D g_clouds : register(t4);
/// @brief What the sun has left after the air above the scene, as one texel. See `atmosphere.hlsl`.
Texture2D g_sunTransmittance : register(t5);
/// @brief What it has left after the cloud layer as well, as one texel. See `clouds.hlsl`.
Texture2D g_cloudSunTransmittance : register(t6);
SamplerState g_sampler : register(s0);
SamplerState g_pointSampler : register(s1);
float HenyeyGreenstein(float cosTheta, float g) {
  float g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}
/// Cornette-Shanks, which is the same lobe corrected to integrate to one. See `atmosphere.hlsl`.
float MiePhase(float cosTheta, float g) {
  float g2 = g * g;
  float numerator = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
  float denominator = 8.0 * PI * (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5);
  return numerator / max(EPSILON, denominator);
}
float RayleighPhase(float cosTheta) {
  return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}
/// @brief Density of the scene's own medium at a height, relative to its density at `u_baseHeight`.
///
/// Exponential going up and flat going down, so a scene whose floor is below the base height does
/// not get a wall of fog in its basement. The flat half matters more than it looks: `baseHeight`
/// defaults to zero and plenty of scenes have geometry under that.
float FogDensity(float height) {
  return u_density * exp(-max(0.0, height - u_baseHeight) * u_heightFalloff);
}
float3 ProjectToShadowMap(float4 positionLight) {
  float3 projected = positionLight.xyz / positionLight.w;
  return float3(projected.x * 0.5 + 0.5, projected.y * -0.5 + 0.5, projected.z);
}
bool OutsideShadowMap(float3 projected) {
  return projected.z < 0.0 || projected.z > 1.0 || any(projected.xy < 0.0) || any(projected.xy > 1.0);
}
/// @brief Whether the sun reaches a point in the air, as one unfiltered comparison.
///
/// One tap rather than the shading pass's filtered block, and not to save time. A shaft is an
/// integral of this along a ray, so the march is already averaging several dozen of these -- the
/// filtering the surface needs to hide a stair-stepped edge is done here by the integration itself,
/// and doing it twice would only soften the edge of the shaft, which is the one thing about it that
/// should stay sharp.
///
/// Outside the map counts as lit. The directional map covers what the scene casts shadows within,
/// and a point beyond it is a point the sun has nothing in the way of.
float DirectionalVisibility(float3 position) {
  if (u_counts.w == 0)
    return 1.0;
  float4 positionLight = mul(u_lightViewProjection, float4(position, 1.0));
  if (positionLight.w <= 0.0)
    return 1.0;
  float3 projected = ProjectToShadowMap(positionLight);
  if (OutsideShadowMap(projected))
    return 1.0;
  float mapDepth = g_shadowMap.SampleLevel(g_pointSampler, projected.xy, 0).r;
  return projected.z - VOLUMETRIC_SHADOW_BIAS > mapDepth ? 0.0 : 1.0;
}
/// @brief The same question of one spot light's layer.
///
/// Outside this map counts as dark, which is the opposite of the directional case and is right for
/// the same reason: a spot light's map covers the whole of its cone, so a point outside it is a
/// point the cone does not reach rather than one nothing is blocking.
float SpotVisibility(uint index, float3 position) {
  if (index >= u_flags.y)
    return 1.0;
  float4 positionLight = mul(u_spotLightViewProjection[index], float4(position, 1.0));
  if (positionLight.w <= 0.0)
    return 0.0;
  float3 projected = ProjectToShadowMap(positionLight);
  if (OutsideShadowMap(projected))
    return 0.0;
  float mapDepth = g_spotShadowMap.SampleLevel(g_pointSampler, float3(projected.xy, index), 0).r;
  return projected.z - VOLUMETRIC_SHADOW_BIAS > mapDepth ? 0.0 : 1.0;
}
/// @brief Interleaved gradient noise, which dithers where each pixel's march begins.
///
/// Without it a low step count draws the shaft as a set of concentric shells -- the step boundaries,
/// which land in the same place for every pixel because every ray starts at the camera. Offsetting
/// each pixel's first step by a fraction of a step turns those shells into a fine grain, and the
/// frame in the hash turns the grain over each frame so it reads as film rather than as pattern.
float Dither(float2 pixel, uint frame) {
  float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
  return frac(magic.z * frac(dot(pixel + (float)(frame & 63u) * 5.588238, magic.xy)));
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
float4 PSMain(PSInput input) : SV_TARGET {
  float3 sceneColor = g_scene.SampleLevel(g_pointSampler, input.uv, 0).rgb;
  float depth = g_depth.SampleLevel(g_pointSampler, input.uv, 0).r;
  // The far end of this pixel's view ray, and how far away it is. A depth of one is the clear value,
  // so nothing was drawn and the ray runs to the sky.
  float4 unprojectedFar = mul(u_inverseViewProjection, float4(input.ndc, 1.0, 1.0));
  float3 farPoint = unprojectedFar.xyz / unprojectedFar.w;
  float3 direction = normalize(farPoint - u_cameraPosition);
  bool isSky = depth >= 1.0;
  float distanceToSurface = u_maxDistance;
  if (!isSky) {
    float4 unprojected = mul(u_inverseViewProjection, float4(input.ndc, depth, 1.0));
    float3 surface = unprojected.xyz / unprojected.w;
    distanceToSurface = length(surface - u_cameraPosition);
  }
  // Whatever this pixel is looking at, with the cloud in front of it where it is looking at sky.
  //
  // The cloud buffer is premultiplied, so this is the whole of the composite and it interpolates:
  // the buffer is gathered at half the viewport and read back with a bilinear tap, which colour and
  // coverage kept apart would not survive. Only on sky pixels, because a cloud is kilometres beyond
  // anything a scene contains and there is nothing it could be in front of otherwise.
  float3 background = sceneColor;
  if (isSky && u_hasClouds != 0) {
    float4 cloud = g_clouds.SampleLevel(g_sampler, input.uv, 0);
    background = cloud.rgb + (1.0 - cloud.a) * sceneColor;
  }
  // The sun as the air sees it: through the atmosphere above the scene, and through whatever cloud
  // is standing in front of it. Both are single texels written by earlier passes -- see the notes on
  // `CSSunTransmittance` in `atmosphere.hlsl` and in `clouds.hlsl` -- so a cloud drifting across the
  // sun takes every shaft in the frame with it, at the cost of one fetch.
  float3 sunAttenuation = float3(1.0, 1.0, 1.0);
  if (u_hasAtmosphere != 0)
    sunAttenuation = g_sunTransmittance.SampleLevel(g_pointSampler, float2(0.5, 0.5), 0).rgb;
  if (u_hasClouds != 0)
    sunAttenuation *= g_cloudSunTransmittance.SampleLevel(g_pointSampler, float2(0.5, 0.5), 0).r;
  float3 sunDirection = normalize(-u_directionalDirection.xyz);
  float cosSun = dot(direction, sunDirection);
  float3 scattering = float3(0.0, 0.0, 0.0);
  // Grey, because the scene's medium removes light without preferring a wavelength -- what colour
  // it has is in what it scatters back, which is `u_albedo`. The atmosphere's extinction below is a
  // `float3` precisely because that one does prefer a wavelength, and is why the sky is blue.
  float transmittance = 1.0;
  // The scene's own medium. Skipped entirely where a scene has said it has none, which is the common
  // case outdoors -- there the atmosphere below is the air, and a second medium on top of it would
  // be haze counted twice.
  if (u_density > 0.0) {
    float span = min(distanceToSurface, u_maxDistance);
    uint steps = max(4u, u_steps);
    float stepLength = span / (float)steps;
    float offset = Dither(input.position.xy, u_frame);
    float phase = HenyeyGreenstein(cosSun, u_anisotropy);
    float3 albedo = saturate(u_albedo);
    for (uint index = 0; index < steps; ++index) {
      float3 position = u_cameraPosition + direction * (stepLength * ((float)index + offset));
      float density = FogDensity(position.y);
      if (density <= 0.0)
        continue;
      // Light arriving here from everywhere at once, which is what keeps the air outside a beam
      // from being black -- a room with one lamp in it should be a bright cone standing in air
      // rather than a bright cone standing in a void. Gathered with no phase term, which is what
      // "from everywhere" means: the isotropic phase integrates to one over the sphere.
      float3 gathered = u_ambient;
      if (u_counts.z != 0) {
        float visibility = DirectionalVisibility(position);
        gathered += visibility * u_directionalDiffuse.rgb * u_directionalIntensity.x * sunAttenuation * phase * u_lightScale;
      }
      for (uint p = 0; p < min(u_counts.x, MAX_POINT_LIGHTS); ++p) {
        float3 offsetToLight = u_pointLights[p].position.xyz - position;
        float distanceToLight = length(offsetToLight);
        float3 a = u_pointLights[p].attenuation.xyz;
        float attenuation = u_pointLights[p].attenuation.w / (a.x + a.y * distanceToLight + a.z * distanceToLight * distanceToLight);
        // No shadow map for a point light, so this is added unoccluded. A lamp in a room casts a
        // glow whether or not anything stands in front of it, which is wrong and is a good deal
        // less wrong than leaving the lamp out of the air entirely.
        float pointPhase = HenyeyGreenstein(dot(direction, offsetToLight / max(distanceToLight, EPSILON)), u_anisotropy);
        gathered += u_pointLights[p].diffuse.rgb * attenuation * pointPhase * u_lightScale;
      }
      for (uint s = 0; s < min(u_counts.y, MAX_SPOT_LIGHTS); ++s) {
        float3 offsetToLight = u_spotLights[s].position.xyz - position;
        float distanceToLight = length(offsetToLight);
        float3 L = offsetToLight / max(distanceToLight, EPSILON);
        float3 a = u_spotLights[s].attenuation.xyz;
        float theta = dot(L, normalize(-u_spotLights[s].direction.xyz));
        float cone = saturate((theta - u_spotLights[s].cutoff.y) / max(u_spotLights[s].cutoff.x - u_spotLights[s].cutoff.y, EPSILON));
        if (cone <= 0.0)
          continue;
        float attenuation = u_spotLights[s].attenuation.w * cone / (a.x + a.y * distanceToLight + a.z * distanceToLight * distanceToLight);
        float spotPhase = HenyeyGreenstein(dot(direction, L), u_anisotropy);
        gathered += SpotVisibility(s, position) * u_spotLights[s].diffuse.rgb * attenuation * spotPhase * u_lightScale;
      }
      // Integrated across the step rather than sampled at its middle, so that a thick medium does
      // not change brightness with the step count. The albedo is what separates this from the cloud
      // march: a droplet scatters nearly everything it removes and a hazy room does not, so the
      // ratio of scattering to extinction stays in the expression here where it cancelled there.
      float stepTransmittance = exp(-density * stepLength);
      scattering += transmittance * gathered * albedo * (1.0 - stepTransmittance);
      transmittance *= stepTransmittance;
    }
  }
  // The atmosphere's air, applied over the same distance and not marched.
  //
  // Nothing is gained by marching it. A scene is tens of units across where the Rayleigh scale
  // height is eight kilometres, so the medium does not measurably change between the near plane and
  // the far one -- which makes the integral through it one that can be written down: constant
  // coefficients give `sunRadiance * scattering / extinction * (1 - exp(-extinction * d))`, exactly.
  // What is approximated is the shadowing, which this does not have and the march above does; a
  // shaft is the scene's own medium's job and the haze is this one's.
  //
  // Surfaces only. A sky pixel is looking at a cubemap that was marched through this very medium
  // from this very altitude, and the cloud in front of it was marched through the layer it sits in
  // -- so both already carry their own air, and hazing them again would be the same kilometres
  // counted twice. What that looks like is a grey veil over the whole background that gets worse
  // the further the draw distance is set.
  if (u_hasAtmosphere != 0 && u_aerialPerspective > 0.0 && !isSky) {
    float kilometres = min(distanceToSurface, u_maxDistance) * max(0.0, u_kilometresPerUnit);
    // The sky is marched with the planet in the way of a low sun, so the transmittance texel already
    // carries the sunset in it. Reading the haze's light out of the same texel is what keeps a
    // scene's distance the same colour as the sky behind it.
    float3 sunRadiance = g_sunTransmittance.SampleLevel(g_pointSampler, float2(0.5, 0.5), 0).rgb * u_sunIntensity;
    float3 inscatterCoefficient = u_rayleighScattering * RayleighPhase(cosSun) + u_mieScattering * MiePhase(cosSun, u_mieAnisotropy);
    float3 extinction = max(u_extinction, EPSILON);
    float3 aerialTransmittance = exp(-extinction * kilometres);
    float3 aerial = sunRadiance * inscatterCoefficient / extinction * (1.0 - aerialTransmittance);
    // Applied behind the scene's own medium, because it is the air further away: the haze in front
    // of a distant hill is seen through whatever dust is in the room, and not the other way round.
    background = background * aerialTransmittance + aerial * u_aerialPerspective;
  }
  return float4(scattering + transmittance * background, 1.0);
}
