// A level surface mirrored in screen space: see `PlanarReflection`. Each pixel the depth puts on the
// plane casts a ray up off it, bent by the ripples, and walks it through the depth buffer until it
// passes behind something on the screen. What it finds there is the reflection.
//
// Mirrors DXReflectionConstants in dx_pipeline.hpp, eleven float4 rows.
cbuffer ReflectionConstants : register(b0) {
  float4x4 u_viewProjection;
  float4x4 u_inverseViewProjection;
  float3 u_cameraPosition;
  float u_height;
  float u_tolerance;
  float u_strength;
  float u_dryReflectance;
  float u_puddleCoverage;
  float u_puddleScale;
  float u_blur;
  float u_ripples;
  float u_rippleDensity;
  float u_maxDistance;
  float u_time;
  float2 u_texelSize;
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
float Hash(float2 p) {
  p = frac(p * float2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return frac(p.x * p.y);
}
float2 Hash2(float2 p) {
  return float2(Hash(p), Hash(p + 17.31));
}
// Value noise, two octaves: smooth patches with a ragged edge, which is what a puddle's outline is.
float Noise(float2 p) {
  float2 cell = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = Hash(cell);
  float b = Hash(cell + float2(1.0, 0.0));
  float c = Hash(cell + float2(0.0, 1.0));
  float d = Hash(cell + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}
float Puddles(float2 p) {
  return Noise(p) * 0.65 + Noise(p * 2.7 + 5.1) * 0.35;
}
// The slope rain puts on the water at a point: in each cell of a grid a drop lands at a place and a
// moment of its own, and a ring runs out from it and dies. The neighbouring cells are asked too, so a
// ring crosses a cell's edge rather than being cut off at it.
float2 Ripples(float2 p, float time) {
  float2 cell = floor(p);
  float2 slope = 0.0;
  [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x) {
      float2 neighbour = cell + float2(x, y);
      float2 random = Hash2(neighbour);
      float age = frac(time * (0.7 + random.x * 0.6) + random.y);
      float2 offset = p - (neighbour + random);
      float distance = length(offset);
      float radius = age * 0.9;
      float wave = sin((distance - radius) * 26.0) * saturate(1.0 - abs(distance - radius) * 12.0) * (1.0 - age) * (1.0 - age);
      slope += distance > 1.0e-4 ? offset / distance * wave : 0.0;
    }
  return slope;
}
float3 WorldPosition(float2 uv, float depth) {
  float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
  float4 world = mul(u_inverseViewProjection, clip);
  return world.xyz / world.w;
}
float3 Project(float3 world) {
  float4 clip = mul(u_viewProjection, float4(world, 1.0));
  float3 ndc = clip.xyz / clip.w;
  return float3(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5, ndc.z);
}
float4 PSReflect(PSInput input) : SV_TARGET {
  float2 uv = input.texture0;
  float4 base = g_source.SampleLevel(g_sampler, uv, 0);
  float depth = g_depth.SampleLevel(g_sampler, uv, 0);
  if (depth >= 1.0)
    return base;
  float3 position = WorldPosition(uv, depth);
  if (abs(position.y - u_height) > u_tolerance)
    return base;
  float puddle = smoothstep(1.0 - u_puddleCoverage - 0.06, 1.0 - u_puddleCoverage + 0.06, Puddles(position.xz * u_puddleScale));
  float wetness = lerp(u_dryReflectance, 1.0, puddle);
  float2 slope = u_ripples * puddle * Ripples(position.xz * u_rippleDensity, u_time) * 0.05;
  float3 normal = normalize(float3(-slope.x, 1.0, -slope.y));
  float3 view = normalize(position - u_cameraPosition);
  float3 ray = reflect(view, normal);
  // Schlick's Fresnel for water, which is most of why a wet street is a mirror when looked along and
  // nearly black when looked down at.
  float cosine = saturate(dot(-view, normal));
  float fresnel = 0.02 + 0.98 * pow(1.0 - cosine, 5.0);
  // Steps that grow as they go, so the ray is fine near the surface, where a figure's feet meet their
  // reflection, and coarse where it crosses open street.
  float step = 0.05;
  float travelled = 0.0;
  float3 marched = position + normal * 0.01;
  float2 hit = -1.0;
  float found = 0.0;
  [loop] for (int i = 0; i < 40 && travelled < u_maxDistance; ++i) {
    float3 next = marched + ray * step;
    float3 projected = Project(next);
    if (any(projected.xy < 0.0) || any(projected.xy > 1.0) || projected.z > 1.0)
      break;
    float scene = g_depth.SampleLevel(g_sampler, projected.xy, 0);
    if (projected.z > scene) {
      // Behind something. Halved back towards the last point that was in front, a few times, to
      // land on the surface rather than a step past it -- unless the thing is far in front of the
      // ray, in which case the ray went behind it and found nothing.
      float3 front = marched;
      float3 back = next;
      [unroll] for (int j = 0; j < 5; ++j) {
        float3 middle = (front + back) * 0.5;
        float3 m = Project(middle);
        if (m.z > g_depth.SampleLevel(g_sampler, m.xy, 0))
          back = middle;
        else
          front = middle;
      }
      float3 landed = Project(back);
      float3 surface = WorldPosition(landed.xy, g_depth.SampleLevel(g_sampler, landed.xy, 0));
      if (distance(surface, back) < 0.6 + step) {
        hit = landed.xy;
        found = 1.0;
      }
      break;
    }
    marched = next;
    travelled += step;
    step *= 1.18;
  }
  if (found <= 0.0)
    return base;
  // Faded where the ray nearly left the picture, so the reflection's edge is never a hard line.
  float2 edge = min(hit, 1.0 - hit);
  float fade = saturate(min(edge.x, edge.y) * 12.0) * saturate(1.0 - travelled / u_maxDistance);
  // Blurred by a few taps across the line of the ray, wider the further the reflected thing is.
  float spread = u_blur * saturate(travelled / 6.0) * 0.012 + u_texelSize.y;
  float3 reflected = g_source.SampleLevel(g_sampler, hit, 0).rgb * 0.4;
  reflected += g_source.SampleLevel(g_sampler, hit + float2(spread, 0.0), 0).rgb * 0.15;
  reflected += g_source.SampleLevel(g_sampler, hit - float2(spread, 0.0), 0).rgb * 0.15;
  reflected += g_source.SampleLevel(g_sampler, hit + float2(0.0, spread * 2.0), 0).rgb * 0.15;
  reflected += g_source.SampleLevel(g_sampler, hit - float2(0.0, spread * 2.0), 0).rgb * 0.15;
  float amount = u_strength * wetness * lerp(0.35, 1.0, fresnel) * fade;
  // Over the surface, which a puddle darkens as it covers it: water hides what is under it.
  float3 surfaceColour = base.rgb * (1.0 - 0.45 * puddle * u_strength);
  return float4(surfaceColour + reflected * amount, base.a);
}
