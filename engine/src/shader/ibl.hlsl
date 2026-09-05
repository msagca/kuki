static const float EPSILON = 1.0e-6;
static const float PI = 3.14159265359;
static const uint BRDF_SAMPLE_COUNT = 1024;
static const float2 INV_ATAN = float2(0.1591, 0.3183);
cbuffer IBLConstants : register(b0) {
  uint u_size;
  uint u_mipLevels;
  uint u_sourceSize;
  uint u_reserved;
  float u_roughness;
  float u_mipLevel;
  uint u_faceSize;
  uint u_padding;
};
Texture2D g_equirect : register(t0);
TextureCube g_cubemap : register(t1);
Texture2DArray g_sourceMip : register(t2);
StructuredBuffer<float4> g_shRead : register(t3);
RWTexture2DArray<float4> g_destination : register(u0);
RWTexture2D<float2> g_lut : register(u1);
RWStructuredBuffer<float4> g_shWrite : register(u2);
SamplerState g_sampler : register(s0);
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
float2 SampleSphericalMap(float3 direction) {
  float2 uv = float2(atan2(direction.z, direction.x), asin(clamp(direction.y, -1.0, 1.0)));
  uv *= INV_ATAN;
  uv += 0.5;
  uv.y = 1.0 - uv.y;
  return uv;
}
float RadicalInverseVdC(uint bits) {
  bits = (bits << 16) | (bits >> 16);
  bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
  bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
  bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
  bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
  return float(bits) * 2.3283064365386963e-10;
}
float2 Hammersley(uint i, uint count) {
  return float2((float)i / (float)count, RadicalInverseVdC(i));
}
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
  float a = roughness * roughness;
  float phi = 2.0 * PI * Xi.x;
  float cosTheta = sqrt(saturate((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y)));
  float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
  float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
  float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
  float3 tangent = normalize(cross(up, N));
  float3 bitangent = cross(N, tangent);
  return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}
float DistributionGGX(float3 N, float3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * denom * denom);
}
[numthreads(8, 8, 1)]
void CSEquirectToCubemap(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size || id.z >= 6)
    return;
  float3 direction = CubeDirectionFromCoord(id, u_size);
  float3 color = g_equirect.SampleLevel(g_sampler, SampleSphericalMap(direction), 0).rgb;
  g_destination[id] = float4(color, 1.0);
}
[numthreads(8, 8, 1)]
void CSDownsampleCubemap(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size || id.z >= 6)
    return;
  float2 uv = (float2(id.xy) + 0.5) / (float)u_size;
  float3 color = g_sourceMip.SampleLevel(g_sampler, float3(uv, (float)id.z), 0).rgb;
  g_destination[id] = float4(color, 1.0);
}
[numthreads(1, 1, 1)]
void CSSHProject(uint3 id : SV_DispatchThreadID) {
  float3 coefficients[9];
  for (int i = 0; i < 9; ++i)
    coefficients[i] = 0.0;
  float totalWeight = 0.0;
  for (uint face = 0; face < 6; ++face)
    for (uint y = 0; y < u_faceSize; ++y)
      for (uint x = 0; x < u_faceSize; ++x) {
        float3 direction = CubeDirectionFromCoord(uint3(x, y, face), u_faceSize);
        float s = 2.0 * ((float)x + 0.5) / (float)u_faceSize - 1.0;
        float t = 2.0 * ((float)y + 0.5) / (float)u_faceSize - 1.0;
        float weight = 1.0 / pow(s * s + t * t + 1.0, 1.5);
        float3 radiance = g_cubemap.SampleLevel(g_sampler, direction, u_mipLevel).rgb;
        float basis[9] = {
          0.282095,
          0.488603 * direction.y,
          0.488603 * direction.z,
          0.488603 * direction.x,
          1.092548 * direction.x * direction.y,
          1.092548 * direction.y * direction.z,
          0.315392 * (3.0 * direction.z * direction.z - 1.0),
          1.092548 * direction.x * direction.z,
          0.546274 * (direction.x * direction.x - direction.y * direction.y)};
        for (int i = 0; i < 9; ++i)
          coefficients[i] += radiance * basis[i] * weight;
        totalWeight += weight;
      }
  float normalization = (4.0 * PI) / totalWeight;
  for (int i = 0; i < 9; ++i)
    g_shWrite[i] = float4(coefficients[i] * normalization, 0.0);
}
float3 EvaluateIrradianceSH(float3 n) {
  const float c1 = 0.429043;
  const float c2 = 0.511664;
  const float c3 = 0.743125;
  const float c4 = 0.886227;
  const float c5 = 0.247708;
  float3 L00 = g_shRead[0].rgb;
  float3 L1m1 = g_shRead[1].rgb;
  float3 L10 = g_shRead[2].rgb;
  float3 L11 = g_shRead[3].rgb;
  float3 L2m2 = g_shRead[4].rgb;
  float3 L2m1 = g_shRead[5].rgb;
  float3 L20 = g_shRead[6].rgb;
  float3 L21 = g_shRead[7].rgb;
  float3 L22 = g_shRead[8].rgb;
  return c1 * L22 * (n.x * n.x - n.y * n.y) + c3 * L20 * n.z * n.z + c4 * L00 - c5 * L20 + 2.0 * c1 * (L2m2 * n.x * n.y + L21 * n.x * n.z + L2m1 * n.y * n.z) + 2.0 * c2 * (L11 * n.x + L1m1 * n.y + L10 * n.z);
}
[numthreads(8, 8, 1)]
void CSIrradiance(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size || id.z >= 6)
    return;
  float3 normal = CubeDirectionFromCoord(id, u_size);
  // Stored as E/PI rather than as the irradiance itself. Everything that samples this map
  // multiplies it straight by albedo, and a Lambertian surface reflects albedo/PI of the irradiance
  // reaching it, so the division has to happen on one side or the other. It happens here because
  // the two other ambient sources already hand their result over in this convention: the fallback
  // sky divides, and `SampleProbeVolume` divides. Leaving it out is what made binding a skybox
  // brighten every diffuse surface by PI against the fallback it replaced.
  //
  // The clamp is for ringing. A second-order fit undershoots opposite a bright source, and this
  // target is floating point, so a negative would survive the write and subtract light downstream.
  g_destination[id] = float4(max(EvaluateIrradianceSH(normal), 0.0) / PI, 1.0);
}
[numthreads(8, 8, 1)]
void CSPrefilter(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size || id.z >= 6)
    return;
  float3 N = CubeDirectionFromCoord(id, u_size);
  float3 V = N;
  float3 prefiltered = 0.0;
  float totalWeight = 0.0;
  uint sampleCount = (uint)lerp(1024.0, 4096.0, u_roughness * u_roughness);
  for (uint i = 0; i < sampleCount; ++i) {
    float2 Xi = Hammersley(i, sampleCount);
    float3 H = ImportanceSampleGGX(Xi, N, u_roughness);
    float3 L = normalize(2.0 * dot(V, H) * H - V);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL > 0.0) {
      float D = DistributionGGX(N, H, u_roughness);
      float NdotH = max(dot(N, H), 0.0);
      float HdotV = max(dot(H, V), 0.0);
      float pdf = D * NdotH / (4.0 * HdotV) + EPSILON;
      float saTexel = 4.0 * PI / (6.0 * (float)u_sourceSize * (float)u_sourceSize);
      float saSample = 1.0 / ((float)sampleCount * pdf + EPSILON);
      float mipLevel = u_roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
      mipLevel = clamp(mipLevel, 0.0, (float)(u_mipLevels - 1));
      prefiltered += g_cubemap.SampleLevel(g_sampler, L, mipLevel).rgb * NdotL;
      totalWeight += NdotL;
    }
  }
  g_destination[id] = float4(prefiltered / max(totalWeight, EPSILON), 1.0);
}
float GeometrySchlickGGXIBL(float NdotV, float roughness) {
  float k = (roughness * roughness) / 2.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmithIBL(float NdotV, float NdotL, float roughness) {
  return GeometrySchlickGGXIBL(NdotV, roughness) * GeometrySchlickGGXIBL(NdotL, roughness);
}
[numthreads(8, 8, 1)]
void CSBRDF(uint3 id : SV_DispatchThreadID) {
  if (id.x >= u_size || id.y >= u_size)
    return;
  float NdotV = ((float)id.x + 0.5) / (float)u_size;
  float roughness = ((float)id.y + 0.5) / (float)u_size;
  float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
  float3 N = float3(0.0, 0.0, 1.0);
  float A = 0.0;
  float B = 0.0;
  for (uint i = 0; i < BRDF_SAMPLE_COUNT; ++i) {
    float2 Xi = Hammersley(i, BRDF_SAMPLE_COUNT);
    float3 H = ImportanceSampleGGX(Xi, N, roughness);
    float3 L = normalize(2.0 * dot(V, H) * H - V);
    float NdotL = max(L.z, 0.0);
    float NdotH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);
    if (NdotL > 0.0) {
      float G = GeometrySmithIBL(NdotV, NdotL, roughness);
      float visibility = (G * VdotH) / (NdotH * NdotV);
      float Fc = pow(1.0 - VdotH, 5.0);
      A += (1.0 - Fc) * visibility;
      B += Fc * visibility;
    }
  }
  g_lut[id.xy] = float2(A, B) / (float)BRDF_SAMPLE_COUNT;
}
