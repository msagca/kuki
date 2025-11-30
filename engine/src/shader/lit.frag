#version 460 core
const float PI = 3.14159265359;
const float EPSILON = 1.0e-6;
const float MAX_REFLECTION_LOD = 4.0;
const uint MAX_POINT_LIGHTS = 8;
in vec3 position;
in vec3 normal;
in vec2 texCoord;
in vec3 tangent;
in vec4 albedo;
in vec4 specular;
in vec4 emissive;
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
        sampler2D specular;
        sampler2D emissive;
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
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform vec3 viewPos;
uniform bool hasSkybox;
uniform bool hasDirLight;
uniform int pointCount;
uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
vec3 GetNormalFromTexture() {
        vec3 tangentNormal = texture(material.normal, texCoord).xyz * 2.0 - 1.0;
        vec3 N = normalize(normal);
        vec3 T = normalize(tangent);
        vec3 B = normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * tangentNormal);
}
float DistributionGGX(vec3 N, vec3 H, float R) {
        float a = R * R;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return nom / denom;
}
float GeometrySchlickGGX(float NdotV, float R) {
        float r = (R + 1.0);
        float k = (r * r) / 8.0;
        float nom = NdotV;
        float denom = NdotV * (1.0 - k) + k;
        return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float R) {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float ggx2 = GeometrySchlickGGX(NdotV, R);
        float ggx1 = GeometrySchlickGGX(NdotL, R);
        return ggx1 * ggx2;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float R) {
        return F0 + (max(vec3(1.0 - R), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 DirLightContribution(DirLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V) {
        vec3 L = normalize(light.direction);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, R);
        float G = GeometrySmith(N, V, L, R);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - vec3(M);
        vec3 diffuse = (A / PI) * light.diffuse;
        vec3 specular = numerator / denominator;
        return (kD * diffuse + specular * light.specular) * NdotL;
}
vec3 PointLightContribution(PointLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, vec3 position) {
        vec3 L = normalize(light.position - position);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, R);
        float G = GeometrySmith(N, V, L, R);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - vec3(M);
        float distance = length(light.position - position);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        vec3 diffuse = (A / PI) * light.diffuse * attenuation;
        vec3 specular = numerator / denominator;
        return (kD * diffuse + specular * light.specular) * NdotL;
}
void main() {
        bool useAlbedoTexture = (textureMask & 0x1) != 0;
        bool useNormalTexture = (textureMask & 0x2) != 0;
        bool useMetalnessTexture = (textureMask & 0x4) != 0;
        bool useOcclusionTexture = (textureMask & 0x8) != 0;
        bool useRoughnessTexture = (textureMask & 0x10) != 0;
        bool useSpecularTexture = (textureMask & 0x20) != 0;
        bool useEmissiveTexture = (textureMask & 0x40) != 0;
        vec3 N = (useNormalTexture) ? GetNormalFromTexture() : normal;
        vec4 A = (useAlbedoTexture) ? texture(material.albedo, texCoord) : albedo;
        vec4 S = (useSpecularTexture) ? texture(material.specular, texCoord) : specular;
        vec4 E = (useEmissiveTexture) ? texture(material.emissive, texCoord) : emissive;
        float O = (useOcclusionTexture) ? texture(material.occlusion, texCoord).r : occlusion;
        float R = (useRoughnessTexture) ? texture(material.roughness, texCoord).g : roughness;
        float M = (useMetalnessTexture) ? texture(material.metalness, texCoord).b : metalness;
        vec3 V = normalize(viewPos - position);
        vec3 reflectDir = reflect(-V, N);
        vec3 F0 = vec3(0.04);
        F0 = mix(F0, A.rgb, vec3(M));
        vec3 Lo = vec3(0.0);
        vec3 ambient;
        if (hasSkybox) {
                vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, R);
                vec3 kS = F;
                vec3 kD = 1.0 - kS;
                kD *= 1.0 - vec3(M);
                vec3 irradiance = texture(irradianceMap, N).rgb;
                vec3 diffuse = irradiance * A.rgb;
                vec3 prefilteredColor = textureLod(prefilterMap, reflectDir, R * MAX_REFLECTION_LOD).rgb;
                vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), R)).rg;
                vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
                ambient = (kD * diffuse + specular) * O;
        } else if (hasDirLight) {
                Lo += DirLightContribution(dirLight, F0, A.rgb, N, M, R, V);
                ambient = dirLight.ambient * A.rgb * O;
        } else
                ambient = vec3(0.03) * A.rgb * O;
        for (int i = 0; i < min(pointCount, MAX_POINT_LIGHTS); ++i)
                Lo += PointLightContribution(pointLights[i], F0, A.rgb, N, M, R, V, position);
        color = vec4(ambient + Lo, A.a);
}
