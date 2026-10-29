// The two fullscreen passes that make the picture read as drawn: `PSInk`, which finds the lines in
// the depth buffer and inks them over the lit scene, and `PSStylize`, which grades the finished
// picture and lays the game's screen effects over it. One file because they share one block of
// constants and one root signature, and are tuned from one component.
//
// Mirrors DXStylizeConstants in dx_pipeline.hpp, nine float4 rows. Every float3 starts a row.
cbuffer StylizeConstants : register(b0) {
  float2 u_texelSize;
  // The two terms of the projection that turn a depth back into a distance: `distance = w / (d + z)`.
  float u_projectionZ;
  float u_projectionW;
  float u_inkThickness;
  float u_inkDepthThreshold;
  float u_inkCreaseThreshold;
  float u_inkFadeDistance;
  float3 u_inkColor;
  float u_inkTint;
  float u_saturation;
  float u_contrast;
  float u_vignette;
  float u_time;
  float3 u_shadowColor;
  float u_flash;
  float3 u_highlightColor;
  float u_impact;
  float3 u_flashColor;
  float u_speedLines;
  float2 u_focus;
  float u_radialBlur;
  float u_aberration;
  float u_aspect;
  float3 u_stylizePadding;
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
// The reciprocal of the distance along the view axis, which is what a depth buffer is an affine
// function of. Kept as a reciprocal because that is the quantity that varies linearly across a
// plane on the screen, so its second difference is nought on any flat surface whatever the angle --
// which is what lets a fold be told from a floor seen at a glancing angle.
float InverseDistance(int2 pixel) {
  float d = g_depth.Load(int3(pixel, 0));
  return (d + u_projectionZ) / u_projectionW;
}
float4 PSInk(PSInput input) : SV_TARGET {
  int2 pixel = int2(input.position.xy);
  float4 base = g_source.Load(int3(pixel, 0));
  float centreDepth = g_depth.Load(int3(pixel, 0));
  // The sky has nothing to outline, and its outline against the scene is drawn on the scene's side.
  if (centreDepth >= 1.0)
    return base;
  float q = InverseDistance(pixel);
  float distance = 1.0 / max(q, 1.0e-6);
  // Thickness in pixels of a picture a thousand and eighty high, so the line keeps its weight
  // relative to the figure as the window is resized.
  int radius = max(1, int(round(u_inkThickness / (u_texelSize.y * 1080.0))));
  const int2 offsets[8] = {int2(1, 0), int2(-1, 0), int2(0, 1), int2(0, -1), int2(1, 1), int2(-1, -1), int2(1, -1), int2(-1, 1)};
  float samples[8];
  float outline = 0.0;
  bool behind = false;
  [unroll] for (int i = 0; i < 8; ++i) {
    samples[i] = InverseDistance(pixel + offsets[i] * radius);
    // A neighbour further away than this pixel by more than the threshold puts this pixel on the
    // edge of something standing in front of it, so the line is drawn here -- on the near side,
    // which is the side a pen would draw it on. A neighbour much nearer means this is the background
    // the edge stands against, which gets no line of its own and none of the fold test below.
    float relative = q / max(samples[i], 1.0e-6) - 1.0;
    outline = max(outline, saturate((relative - u_inkDepthThreshold) / u_inkDepthThreshold));
    behind = behind || -relative > u_inkDepthThreshold;
  }
  float crease = 0.0;
  if (!behind) {
    // The second difference along each axis, relative to the pixel's own value and divided by the
    // reach, so the measure of a fold does not change with the thickness or the distance it is seen
    // at: a right angle seen corner on comes to about one.
    float folded = max(abs(samples[0] + samples[1] - 2.0 * q), abs(samples[2] + samples[3] - 2.0 * q));
    folded = max(folded, 0.7 * max(abs(samples[4] + samples[5] - 2.0 * q), abs(samples[6] + samples[7] - 2.0 * q)));
    float measure = folded / max(q, 1.0e-6) / float(radius) * 1000.0;
    crease = saturate((measure - u_inkCreaseThreshold) / max(u_inkCreaseThreshold, 1.0e-3));
  }
  float weight = max(outline, crease * 0.85);
  weight *= 1.0 - smoothstep(u_inkFadeDistance * 0.5, u_inkFadeDistance, distance);
  // Dark, and taking the colour of what it is drawn across: a line across a red sleeve is a dark red.
  float3 ink = lerp(u_inkColor, base.rgb * 0.12, u_inkTint);
  return float4(lerp(base.rgb, ink, weight), base.a);
}
float Hash(float2 p) {
  p = frac(p * float2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return frac(p.x * p.y);
}
float Luminance(float3 c) {
  return dot(c, float3(0.2126, 0.7152, 0.0722));
}
float4 PSStylize(PSInput input) : SV_TARGET {
  float2 uv = input.texture0;
  float2 offset = uv - u_focus;
  float3 colour = g_source.SampleLevel(g_sampler, uv, 0).rgb;
  // The smear: the picture averaged along the line out of the focus, further the further from it.
  if (u_radialBlur > 0.0) {
    float3 sum = colour;
    const int TAPS = 12;
    [unroll] for (int i = 1; i < TAPS; ++i) {
      float scale = 1.0 - u_radialBlur * 0.15 * float(i) / float(TAPS);
      sum += g_source.SampleLevel(g_sampler, u_focus + offset * scale, 0).rgb;
    }
    colour = sum / float(TAPS);
  }
  // The channels pulled apart along the same line, by nothing at the focus and the most at the edge.
  if (u_aberration > 0.0) {
    float2 shift = offset * (u_aberration * u_texelSize.y * 2.0);
    colour.r = g_source.SampleLevel(g_sampler, uv + shift, 0).r;
    colour.b = g_source.SampleLevel(g_sampler, uv - shift, 0).b;
  }
  // The grade. Saturation about the pixel's own grey, contrast about the middle of the range, then
  // the darks and the lights each pushed towards their own colour by as much as they are dark or light.
  float lum = Luminance(colour);
  colour = lerp(lum.xxx, colour, u_saturation);
  colour = (colour - 0.5) * u_contrast + 0.5;
  colour += (u_shadowColor - 0.5) * (1.0 - lum) * 0.5 + (u_highlightColor - 0.5) * lum * 0.5;
  float2 centred = (uv - 0.5) * float2(u_aspect, 1.0);
  colour *= 1.0 - u_vignette * smoothstep(0.3, 1.0, length(centred) * 1.25);
  // Speed lines: thin wedges round the focus, a new set every few hundredths of a second, clear of the
  // middle so the thing they point at stays in sight.
  if (u_speedLines > 0.0) {
    float2 aspected = offset * float2(u_aspect, 1.0);
    float angle = atan2(aspected.y, aspected.x) / 6.28318530718 + 0.5;
    float slot = floor(angle * 180.0);
    float frame = floor(u_time * 24.0);
    float chosen = step(1.0 - 0.35 * u_speedLines, Hash(float2(slot, frame)));
    float across = abs(frac(angle * 180.0) - 0.5) * 2.0;
    float reach = Hash(float2(slot, frame + 7.0)) * 0.25 + 0.3;
    float streak = chosen * (1.0 - smoothstep(0.2, 0.9, across)) * smoothstep(reach, reach + 0.3, length(aspected));
    colour = lerp(colour, float3(1.0, 1.0, 1.0), streak * saturate(u_speedLines));
  }
  // The impact frame: two tones, inverted, with the dark half dyed the colour of a hit.
  if (u_impact > 0.0) {
    float tone = smoothstep(0.38, 0.45, Luminance(saturate(colour)));
    float3 frame = lerp(float3(1.0, 0.96, 0.9), float3(0.12, 0.02, 0.05), tone);
    colour = lerp(colour, frame, saturate(u_impact));
  }
  colour = lerp(colour, u_flashColor, saturate(u_flash));
  return float4(saturate(colour), 1.0);
}
