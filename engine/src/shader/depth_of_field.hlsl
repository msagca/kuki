// A shallow focus over the finished picture: every pixel gathers the pixels round it whose blur is
// wide enough to reach it, so what is out of focus spreads and what is in focus does not.
//
// Gathered rather than scattered, in one pass at full size. Each tap is weighted by whether its own
// circle of confusion covers the distance to the centre, not by the centre's, which is what keeps a
// sharp thing's edge sharp against a soft background: a blurred hill behind a house in focus gathers
// nothing from the house, because the house's circle is too small to reach it, and so is not haloed
// with its colour. A tap behind the centre is held to the centre's circle as well, so a soft
// background never spreads over a sharper thing in front of it; a soft foreground does spread over
// what is behind it, as a near branch out of focus does through a lens.
//
// Mirrors DXFocusConstants in dx_pipeline.hpp, three float4 rows.
cbuffer FocusConstants : register(b0) {
  float2 u_texelSize;
  // Where the focus is read when `u_focusDistance` is nought, as a place on the picture.
  float2 u_focusPoint;
  // The terms of the inverse projection that turn a depth into a view space z:
  // `z = (x * d + y) / (z * d + w)`, for either kind of camera.
  float4 u_depthTerms;
  float u_focusDistance;
  float u_focusRange;
  float u_transition;
  // The widest circle, in pixels of this picture.
  float u_maxBlur;
};
Texture2D g_source : register(t0);
Texture2D<float> g_depth : register(t1);
SamplerState g_sampler : register(s0);
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
// Forty taps over a disc, enough that the widest blur allowed has no gaps between them for a
// bilinear fetch to fall through.
static const int FOCUS_TAPS = 40;
static const float GOLDEN_ANGLE = 2.39996323;
int2 Size() {
  return int2(round(1.0 / u_texelSize));
}
float Distance(float depth) {
  return -(u_depthTerms.x * depth + u_depthTerms.y) / (u_depthTerms.z * depth + u_depthTerms.w);
}
float DistanceAt(int2 pixel) {
  return Distance(g_depth.Load(int3(clamp(pixel, int2(0, 0), Size() - 1), 0)));
}
// The radius in pixels of the blur at a distance, given the distance in focus.
float Circle(float distance, float focus) {
  float away = abs(distance - focus) / focus;
  return u_maxBlur * saturate((away - u_focusRange) / max(u_transition, 1.0e-3));
}
// The distance at the focus point, read here on every pixel rather than once on the CPU: a readback
// would put the focus a frame or more behind the camera, and five loads of one place are in cache for
// every pixel after the first. Taken as the nearest of the five that is not sky, so a focus point on
// the edge of a roof focuses on the roof; nought when all five are sky, which leaves the picture sharp.
float AutoFocus() {
  int2 centre = int2(u_focusPoint / u_texelSize);
  int spread = max(int(4.0 / (u_texelSize.y * 1080.0)), 1);
  const int2 offsets[5] = {int2(0, 0), int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1)};
  float nearest = 0.0;
  [unroll] for (int i = 0; i < 5; ++i) {
    int2 at = clamp(centre + offsets[i] * spread, int2(0, 0), Size() - 1);
    float depth = g_depth.Load(int3(at, 0));
    if (depth >= 1.0)
      continue;
    float distance = Distance(depth);
    nearest = nearest > 0.0 ? min(nearest, distance) : distance;
  }
  return nearest;
}
float4 PSFocus(PSInput input) : SV_TARGET {
  int2 pixel = int2(input.position.xy);
  float4 base = g_source.Load(int3(pixel, 0));
  float focus = u_focusDistance > 0.0 ? u_focusDistance : AutoFocus();
  if (focus <= 0.0)
    return base;
  float distance = DistanceAt(pixel);
  float circle = Circle(distance, focus);
  float3 sum = base.rgb;
  float weights = 1.0;
  [unroll] for (int i = 0; i < FOCUS_TAPS; ++i) {
    // Spread evenly over the disc's area, so every ring of it is gathered as densely as the next.
    float along = sqrt((float(i) + 0.5) / float(FOCUS_TAPS)) * u_maxBlur;
    float angle = float(i) * GOLDEN_ANGLE;
    float2 offset = float2(cos(angle), sin(angle)) * along;
    float tapDistance = DistanceAt(pixel + int2(round(offset)));
    float tapCircle = Circle(tapDistance, focus);
    if (tapDistance > distance)
      tapCircle = min(tapCircle, circle);
    // Whether the tap's circle reaches the centre, with half a pixel of give so a circle that just
    // reaches it does not switch on and off between neighbours.
    float weight = saturate(tapCircle - along + 0.5);
    sum += g_source.SampleLevel(g_sampler, (input.position.xy + offset) * u_texelSize, 0).rgb * weight;
    weights += weight;
  }
  return float4(sum / weights, base.a);
}
