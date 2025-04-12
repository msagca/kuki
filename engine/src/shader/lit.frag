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
uniform bool useAlbedoTexture;
uniform bool useNormalTexture;
uniform bool useMetalnessTexture;
uniform bool useOcclusionTexture;
uniform bool useRoughnessTexture;
uniform bool dirExists;
uniform DirLight dirLight;
uniform int pointCount;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
const float PI = 3.1415927;
vec3 GetNormalFromTexture() {
    vec3 tangentNormal = texture(material.normal, texCoord).xyz * 2.0 - 1.0;
    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(vec3 N, vec3 H, float R) {
    float a = R * R;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}
float GeometrySchlickGGX(float NdotV, float R) {
    float r = (R + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float R) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, R);
    float ggx1 = GeometrySchlickGGX(NdotL, R);
    return ggx1 * ggx2;
}
vec3 CalculateDirectionalLight(DirLight light, vec3 A, vec3 N, vec3 M, vec3 O, vec3 R, vec3 V) {
    vec3 L = normalize(light.direction);
    vec3 H = normalize(V + L);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, A, M);
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
    vec3 radiance = light.diffuse;
    return (kD * A / PI + specular) * radiance * NdotL + light.ambient * A * O;
}
vec3 CalculatePointLight(PointLight light, vec3 A, vec3 N, vec3 M, vec3 O, vec3 R, vec3 V, vec3 fragPos) {
    vec3 L = normalize(light.position - fragPos);
    vec3 H = normalize(V + L);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, A, M);
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
    vec3 radiance = light.diffuse * attenuation;
    return (kD * A / PI + specular) * radiance * NdotL + light.ambient * A * O * attenuation;
}
void main() {
    vec3 A = (useAlbedoTexture) ? texture(material.albedo, texCoord).rgb : albedo;
    vec3 N = (useNormalTexture) ? GetNormalFromTexture() : normal;
    vec3 M = (useMetalnessTexture) ? texture(material.metalness, texCoord).rgb : vec3(metalness);
    vec3 O = (useOcclusionTexture) ? texture(material.occlusion, texCoord).rgb : vec3(occlusion);
    vec3 R = (useRoughnessTexture) ? texture(material.roughness, texCoord).rgb : vec3(roughness);
    vec3 V = normalize(viewPos - position);
    vec3 finalColor = vec3(0.0);
    if (dirExists)
        finalColor += CalculateDirectionalLight(dirLight, A, N, M, O, R, V);
    for (int i = 0; i < pointCount; ++i)
        finalColor += CalculatePointLight(pointLights[i], A, N, M, O, R, V, position);
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    color = vec4(finalColor, 1.0);
}
