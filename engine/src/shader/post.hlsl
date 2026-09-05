cbuffer PostConstants : register(b0) {
  float u_parameter;
  float2 u_texelSize;
  uint u_horizontal;
  float u_exposure;
  uint u_toneMapper;
  uint u_raw;
};
static const uint TONE_MAPPER_NONE = 0;
static const uint TONE_MAPPER_REINHARD = 1;
static const uint TONE_MAPPER_ACES = 2;
static const uint TONE_MAPPER_AGX = 3;
Texture2D g_source : register(t0);
Texture2D g_second : register(t1);
SamplerState g_sampler : register(s0);
static const float BLUR_WEIGHT[3] = {0.2270270270, 0.3162162162, 0.0702702703};
static const float BLUR_OFFSET[3] = {0.0, 1.3846153846, 3.2307692308};
struct PSInput {
  float4 position : SV_POSITION;
  float2 texture0 : TEXCOORD0;
};
PSInput VSMain(uint id : SV_VertexID) {
  PSInput output;
  output.texture0 = float2((id << 1) & 2, id & 2);
  output.position = float4(output.texture0 * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
  return output;
}
float4 PSCopy(PSInput input) : SV_TARGET {
  return g_source.Sample(g_sampler, input.texture0);
}
// Radiance to display, which is the clip and the transfer curve every operator but AgX still needs.
float3 Encode(float3 radiance) {
  return pow(saturate(radiance), 1.0 / u_parameter);
}
float3 ACESNarkowicz(float3 x) {
  return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}
// Sixth-order fit to AgX's contrast sigmoid, to a mean squared error around 3.7e-06, which is far
// below a display's smallest step and so is exact as far as the image is concerned.
float3 AgXContrast(float3 x) {
  float3 x2 = x * x;
  float3 x4 = x2 * x2;
  return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
// AgX works in log space rather than on the radiance directly: the scene is rotated into a slightly
// desaturated working set of primaries, mapped onto a fixed window of log2 stops, run through the
// sigmoid, and rotated back. Compressing in log means a colour approaches white by losing saturation
// rather than by having its largest channel clipped, and that is what holds the hue steady.
//
// No look transform is layered on top, so this is the neutral AgX rather than the punchier curve
// Blender shows by default. Middle grey leaves it at about 0.50.
//
// The result leaves display encoded, because the sigmoid maps onto the display's range directly.
// Reference implementations then raise it to 2.2 to undo that, since they hand the result to an sRGB
// render target that applies the curve again. This pipeline writes to a linear target and encodes by
// hand, so both of those steps are dropped rather than performed and immediately undone.
//
// The two rotations are written with the same coefficients in the same order as the OpenGL port, and
// so must be multiplied with the vector on the left. A `float3x3` initialiser fills rows where GLSL's
// `mat3` fills columns, which makes these each other's transpose; `mul(v, m)` cancels that out, and
// `mul(m, v)` would silently apply the wrong rotation rather than fail to compile.
float3 AgX(float3 radiance) {
  const float3x3 toAgX = float3x3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
  const float3x3 fromAgX = float3x3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
  const float minEV = -12.47393;
  const float maxEV = 4.026069;
  // The rotation carries negative off-diagonal terms, so a saturated primary can leave it just below
  // zero. `log2` of a negative is a NaN, and a NaN survives the clamp that follows and spreads back
  // through the second rotation, so it is cut off here rather than tested for afterwards.
  float3 working = max(mul(radiance, toAgX), 0.0);
  working = (clamp(log2(working), minEV, maxEV) - minEV) / (maxEV - minEV);
  return saturate(mul(AgXContrast(working), fromAgX));
}
float4 PSToneMapping(PSInput input) : SV_TARGET {
  // Exposure is a plain scale in stops, so a step of one doubles the light handed to the curve. It
  // belongs here rather than in the lighting because it describes the camera rather than the scene,
  // and applying it once at the end leaves every earlier pass working in the radiance the lights
  // actually emit.
  float3 sourced = g_source.Sample(g_sampler, input.texture0).rgb;
  // Straight through while a debug view is up. What the scene pass wrote in that case is a quantity
  // rather than a radiance, and exposing and curving a quantity gives a picture of the number rather
  // than the number -- which defeats the half of a debug view that consists of reading a value off
  // the screen and judging whether it is the one it should be.
  if (u_raw != 0)
    return float4(sourced, 1.0);
  float3 radiance = sourced * exp2(u_exposure);
  float3 display;
  if (u_toneMapper == TONE_MAPPER_REINHARD)
    display = Encode(radiance / (radiance + 1.0));
  else if (u_toneMapper == TONE_MAPPER_ACES)
    display = Encode(ACESNarkowicz(radiance));
  else if (u_toneMapper == TONE_MAPPER_AGX)
    // Deliberately not through `Encode`: AgX ends in display space already. See `AgX`.
    display = AgX(radiance);
  else
    display = Encode(radiance);
  return float4(display, 1.0);
}
float4 PSBrightPass(PSInput input) : SV_TARGET {
  // Nothing glows while a debug view is up. The threshold sits at half, and most of what these views
  // write is a nought-to-one quantity that spends much of the screen above it -- so left alone the
  // bloom would take a white patch of occlusion and smear it over the darker reading beside it,
  // which is the one comparison the view exists to let you make. Returning black here empties the
  // blur and leaves the combine adding nothing, without taking either pass out of the graph.
  if (u_raw != 0)
    return float4(0.0, 0.0, 0.0, 1.0);
  float4 source = g_source.Sample(g_sampler, input.texture0);
  // Thresholded on the exposed image but extracted from the unexposed one, so that what glows is
  // decided in the brightness the viewer ends up seeing while the extracted radiance stays in the
  // space the rest of the chain works in. Testing before exposure instead would hold the bloom still
  // while the image around it brightened, which reads as the effect coming loose from the scene.
  float luminance = dot(source.rgb * exp2(u_exposure), float3(0.2126, 0.7152, 0.0722));
  float contribution = saturate(luminance - u_parameter);
  return float4(source.rgb * contribution, 1.0);
}
float4 PSBlur(PSInput input) : SV_TARGET {
  float3 result = g_source.Sample(g_sampler, input.texture0).rgb * BLUR_WEIGHT[0];
  for (int i = 1; i < 3; ++i) {
    float2 delta = u_horizontal != 0 ? float2(u_texelSize.x * BLUR_OFFSET[i], 0.0) : float2(0.0, u_texelSize.y * BLUR_OFFSET[i]);
    result += g_source.Sample(g_sampler, input.texture0 + delta).rgb * BLUR_WEIGHT[i];
    result += g_source.Sample(g_sampler, input.texture0 - delta).rgb * BLUR_WEIGHT[i];
  }
  return float4(result, 1.0);
}
float4 PSBloom(PSInput input) : SV_TARGET {
  float3 scene = g_source.Sample(g_sampler, input.texture0).rgb;
  float3 bright = g_second.Sample(g_sampler, input.texture0).rgb;
  // Stays in radiance. The tone mapping pass owns the curve now, and compressing here as well would
  // run the image through two of them, the first with no exposure applied and no way to choose it.
  return float4(scene + bright * u_parameter, 1.0);
}
