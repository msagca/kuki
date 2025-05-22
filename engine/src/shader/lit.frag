#version 460 core
#define MAX_POINT_LIGHTS 8
in vec3 position;
in vec3 normal;
in vec2 texCoord;
in vec3 tangent;
in vec3 albedo;
in float metalness;
in float occlusion;
in float roughness;
flat in int textureMask;
out vec4 color;
struct Material {
    sampler2D albedo;
    sampler2D normal;
    sampler2D metalness;
    sampler2D occlusion;
    sampler2D roughness;
};
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
uniform vec3 viewPos;
uniform Material material;
uniform bool hasDirLight;
uniform DirLight dirLight;
uniform int pointCount;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform bool hasSkybox;
const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;
vec3 GetNormalFromTexture() {
    vec3 tangentNormal = texture(material.normal, texCoord).xyz * 2.0 - 1.0;
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 DirLightContribution(DirLight light, vec3 F0, vec3 A, vec3 N, vec3 M, vec3 R, vec3 V) {
    vec3 L = normalize(light.direction);
    vec3 H = normalize(V + L);
    vec3 radiance = light.diffuse;
    float NDF = DistributionGGX(N, H, R.r);
    float G = GeometrySmith(N, V, L, R.r);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - M;
    float NdotL = max(dot(N, L), 0.0);
    return (kD * A / PI + specular) * radiance * NdotL;
}
vec3 PointLightContribution(PointLight light, vec3 F0, vec3 A, vec3 N, vec3 M, vec3 R, vec3 V, vec3 position) {
    vec3 L = normalize(light.position - position);
    vec3 H = normalize(V + L);
    float distance = length(light.position - position);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    vec3 radiance = light.diffuse * attenuation;
    float NDF = DistributionGGX(N, H, R.r);
    float G = GeometrySmith(N, V, L, R.r);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - M;
    float NdotL = max(dot(N, L), 0.0);
    return (kD * A / PI + specular) * radiance * NdotL;
}
void main() {
    bool useAlbedoTexture = (textureMask & 0x1) != 0;
    bool useNormalTexture = (textureMask & 0x2) != 0;
    bool useMetalnessTexture = (textureMask & 0x4) != 0;
    bool useOcclusionTexture = (textureMask & 0x8) != 0;
    bool useRoughnessTexture = (textureMask & 0x10) != 0;
    vec3 A = (useAlbedoTexture) ? texture(material.albedo, texCoord).rgb : albedo;
    vec3 N = (useNormalTexture) ? GetNormalFromTexture() : normal;
    vec3 M = (useMetalnessTexture) ? texture(material.metalness, texCoord).rgb : vec3(metalness);
    vec3 O = (useOcclusionTexture) ? texture(material.occlusion, texCoord).rgb : vec3(occlusion);
    vec3 R = (useRoughnessTexture) ? texture(material.roughness, texCoord).rgb : vec3(roughness);
    vec3 V = normalize(viewPos - position);
    vec3 reflectDir = reflect(-V, N);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, A, M);
    vec3 Lo = vec3(0.0);
    if (hasDirLight)
        Lo += DirLightContribution(dirLight, F0, A, N, M, R, V);
    for (int i = 0; i < min(pointCount, MAX_POINT_LIGHTS); ++i)
        Lo += PointLightContribution(pointLights[i], F0, A, N, M, R, V, position);
    vec3 ambient;
    if (hasSkybox) {
        vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, R.r);
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - M;
        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse = irradiance * A;
        vec3 prefilteredColor = textureLod(prefilterMap, reflectDir, R.r * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), R.r)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
        ambient = (kD * diffuse + specular) * O;
    } else if (hasDirLight)
        ambient = dirLight.ambient * A * O;
    else
        ambient = vec3(0.03) * A * O;
    color = vec4(ambient + Lo, 1.0);
}
