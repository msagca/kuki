// How much of the sky each pixel's surroundings hide from it, from the depth prepass alone, and the
// blur that makes the estimate smooth. `PSOcclusion` writes a noisy estimate and `PSOcclusionBlur`
// runs twice over it, across and then down, into what the scene pass reads.
//
// The estimate looks for occluders over a disc whose size is set in scene units and projected per
// pixel, so a crease is the same width of shade at any distance, and the blur is sized to the same
// disc. The screen space term this replaces did neither: it looked over a fixed number of pixels and
// blurred over a window fixed at four, so a crease seen from far enough away was sharper than the
// window and printed the per-pixel rotation of its samples into the picture. Here a crease seen from
// that far has a disc too small to sample, and its shade fades out before the noise can show.
//
// Mirrors DXOcclusionConstants in dx_pipeline.hpp, seven float4 rows.
cbuffer OcclusionConstants : register(b0) {
  float4x4 u_inverseProjection;
  // The projection's scale on each axis and the two terms that give the clip space w of a point at
  // a view space z, `w = z * u_projectionW.x + u_projectionW.y`: minus z for a perspective camera,
  // one for an orthographic one. Together they say how many pixels a length at a depth covers.
  float2 u_projectionScale;
  float2 u_projectionW;
  float2 u_texelSize;
  float u_radius;
  float u_intensity;
  float u_power;
  // The widest the disc may be on the screen, in pixels, which bounds the cost near the camera.
  float u_maxPixels;
  uint u_horizontal;
  float u_occlusionPadding;
};
Texture2D<float> g_depth : register(t0);
Texture2D<float> g_occlusion : register(t1);
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
// Twelve samples: enough that with the rotation below and the blur after a crease is a smooth shade,
// and few enough that the pass costs less than the sun's soft shadow does per pixel.
static const int OCCLUSION_SAMPLES = 12;
// The turn between one sample and the next, the golden angle, so no two line up whatever the count.
static const float GOLDEN_ANGLE = 2.39996323;
// What a sample has to rise above the surface's own plane, as the cosine of its angle to it, before
// it counts. The depth buffer's own steps put a flat floor's samples a hair above or below the plane,
// and without this a floor seen at a grazing angle would shade itself.
static const float OCCLUSION_BIAS = 0.06;
int2 Size() {
  return int2(round(1.0 / u_texelSize));
}
float3 ViewPosition(int2 pixel, float depth) {
  float2 uv = (float2(pixel) + 0.5) * u_texelSize;
  float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
  float4 view = mul(u_inverseProjection, clip);
  return view.xyz / view.w;
}
float3 ViewPosition(int2 pixel) {
  return ViewPosition(pixel, g_depth.Load(int3(clamp(pixel, int2(0, 0), Size() - 1), 0)));
}
// The width on the screen, in pixels, of the occlusion radius at a view space depth.
float PixelRadius(float viewZ) {
  float w = max(viewZ * u_projectionW.x + u_projectionW.y, 1.0e-4);
  return u_radius * u_projectionScale.y / w * 0.5 / u_texelSize.y;
}
// A number from nought to one that changes from pixel to pixel in a pattern a small blur takes out:
// interleaved gradient noise, whose neighbours in any three by three are spread over the whole range.
float InterleavedGradientNoise(float2 pixel) {
  return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}
float4 PSOcclusion(PSInput input) : SV_TARGET {
  int2 pixel = int2(input.position.xy);
  float depth = g_depth.Load(int3(pixel, 0));
  if (depth >= 1.0)
    return 1.0;
  float3 P = ViewPosition(pixel, depth);
  // The surface's plane, from its neighbours on whichever side is nearer in depth, so a pixel on
  // the edge of something takes the plane of the thing and not of the step down to what is behind.
  float3 left = ViewPosition(pixel - int2(1, 0));
  float3 right = ViewPosition(pixel + int2(1, 0));
  float3 up = ViewPosition(pixel - int2(0, 1));
  float3 down = ViewPosition(pixel + int2(0, 1));
  float3 dx = abs(right.z - P.z) < abs(P.z - left.z) ? right - P : P - left;
  float3 dy = abs(down.z - P.z) < abs(P.z - up.z) ? down - P : P - up;
  // Turned to face the camera, which is down minus z from the origin for a perspective camera and
  // straight down minus z for an orthographic one. On the picture's edge, where a neighbour is the
  // pixel itself and there is no plane to find, the surface is taken to face the camera.
  float3 toward = u_projectionW.x != 0.0 ? -P : float3(0.0, 0.0, 1.0);
  float3 across = cross(dy, dx);
  float3 N = dot(across, across) > 1.0e-20 ? normalize(across) : normalize(toward);
  if (dot(N, toward) < 0.0)
    N = -N;
  float pixels = PixelRadius(P.z);
  // Under a pixel or two the disc holds no sample that is not this pixel or its neighbour, and the
  // estimate is the depth buffer's noise rather than the scene's shape: fade it out instead.
  float reach = saturate((pixels - 1.0) / 3.0);
  if (reach <= 0.0)
    return 1.0;
  pixels = min(pixels, u_maxPixels);
  // The radius the disc actually covers, which is less than asked for where it was clamped.
  float radius = u_radius * pixels / max(PixelRadius(P.z), 1.0e-4);
  float radius2 = radius * radius;
  float rotation = InterleavedGradientNoise(input.position.xy) * 6.28318530718;
  float sum = 0.0;
  [unroll] for (int i = 0; i < OCCLUSION_SAMPLES; ++i) {
    // Nearer the centre more often than further out, since what touches a point shades it most.
    float along = (float(i) + 0.5) / float(OCCLUSION_SAMPLES);
    float angle = float(i) * GOLDEN_ANGLE + rotation;
    int2 offset = int2(round(float2(cos(angle), sin(angle)) * along * pixels));
    int2 at = pixel + offset;
    if (any(at < 0) || any(at >= Size()))
      continue;
    float sampleDepth = g_depth.Load(int3(at, 0));
    if (sampleDepth >= 1.0)
      continue;
    float3 v = ViewPosition(at, sampleDepth) - P;
    float vv = dot(v, v);
    // How far above the plane the sample stands, as the cosine of its angle to it, and less the
    // further it is: anything past the radius -- a wall far behind the edge of a roof -- is nothing.
    float rise = dot(v, N) * rsqrt(vv + 1.0e-6) - OCCLUSION_BIAS;
    float falloff = saturate(1.0 - vv / radius2);
    sum += max(rise, 0.0) * falloff * falloff;
  }
  // Half the disc is all a surface can have over it, so twice the mean is the share of the sky that
  // is hidden: about a third in the corner of a floor and a wall.
  float hidden = sum * 2.0 / float(OCCLUSION_SAMPLES);
  float open = pow(saturate(1.0 - hidden * u_intensity), u_power);
  return lerp(1.0, open, reach);
}
// One direction of the blur. As wide as half the disc and never under two pixels, which is the
// smallest window the noise above evens out across; weighted against the centre's plane, so the
// shade of a floor is not spread up the wall standing on it, and a sloped roof seen at a slant still
// counts its own pixels however fast its depth changes across them.
float4 PSOcclusionBlur(PSInput input) : SV_TARGET {
  int2 pixel = int2(input.position.xy);
  float depth = g_depth.Load(int3(pixel, 0));
  float centre = g_occlusion.Load(int3(pixel, 0));
  if (depth >= 1.0)
    return 1.0;
  int2 step = u_horizontal != 0 ? int2(1, 0) : int2(0, 1);
  float z = ViewPosition(pixel, depth).z;
  float before = ViewPosition(pixel - step).z;
  float after = ViewPosition(pixel + step).z;
  float slope = abs(after - z) < abs(z - before) ? after - z : z - before;
  // How far off the plane a neighbour may be before it is another surface: a share of the radius,
  // which is what says how far apart two things have to be for one not to shade the other.
  float tolerance = max(u_radius * 0.25, abs(z) * 0.002);
  int extent = clamp(int(PixelRadius(z) * 0.5), 2, 8);
  float sigma = max(float(extent) * 0.5, 1.0);
  float sum = centre;
  float weights = 1.0;
  for (int k = -extent; k <= extent; ++k) {
    if (k == 0)
      continue;
    int2 at = clamp(pixel + step * k, int2(0, 0), Size() - 1);
    float sampleDepth = g_depth.Load(int3(at, 0));
    if (sampleDepth >= 1.0)
      continue;
    float off = abs(ViewPosition(at, sampleDepth).z - (z + slope * float(k)));
    float plane = saturate(1.0 - off / tolerance);
    float weight = exp(-float(k * k) / (2.0 * sigma * sigma)) * plane * plane;
    sum += g_occlusion.Load(int3(at, 0)) * weight;
    weights += weight;
  }
  return sum / weights;
}
