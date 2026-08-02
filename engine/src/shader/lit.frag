#version 460 core
const float EPSILON = 1.0e-6;
const float MAX_REFLECTION_LOD = 6.0; // NOTE: must equal (mip levels of u_prefilterMap - 1), see GLRenderer::LoadAsset<TextureAsset>
const float PI = 3.14159265359;
const uint MAX_POINT_LIGHTS = 8;
const uint MAX_SPOT_LIGHTS = 8;
flat in int v_textureMask;
flat in uint v_entityId;
in float v_metalness;
in float v_occlusion;
in float v_roughness;
in vec2 v_texCoords;
in vec3 v_normal;
in vec3 v_position;
in vec4 v_positionL;
in vec3 v_tangent;
in vec4 v_albedo;
in vec4 v_emissive;
in vec4 v_specular;
out vec4 color;
layout(location = 1) out vec4 entityColor;
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
        float intensity;
        mat4 view;
        mat4 projection;
};
struct PointLight {
        vec3 position;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
        float intensity;
        float constant;
        float linear;
        float quadratic;
};
struct SpotLight {
        vec3 position;
        vec3 direction;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
        float intensity;
        float constant;
        float linear;
        float quadratic;
        float innerCutoff;
        float outerCutoff;
};
uniform DirLight u_dirLight;
uniform Material u_material;
uniform PointLight u_pointLights[MAX_POINT_LIGHTS];
uniform SpotLight u_spotLights[MAX_SPOT_LIGHTS];
uniform bool u_hasBRDF;
uniform bool u_hasDirLight;
uniform bool u_hasIrradianceMap;
uniform bool u_hasPrefilterMap;
uniform bool u_hasSkybox;
uniform sampler2D u_brdfLUT;
uniform sampler2D u_shadowMap;
uniform sampler2DArray u_spotShadowMap;
uniform mat4 u_spotLightViewProj[MAX_SPOT_LIGHTS];
uniform samplerCube u_irradianceMap;
uniform samplerCube u_prefilterMap;
uniform uint u_pointCount;
uniform uint u_spotCount;
uniform vec3 u_viewPos;
float DistributionGGX(vec3, vec3, float);
float GeometrySchlickGGX(float, float);
float GeometrySmith(vec3, vec3, vec3, float);
float ShadowAmount(sampler2D, vec3, vec4, vec3);
float ShadowCompare(vec3, float, vec3, vec3);
float SpotShadowAmount(sampler2DArray, float, mat4, vec3, vec3, vec3);
vec2 FallbackBRDF(float, float);
vec3 DirLightContribution(DirLight, vec3, vec3, vec3, float, float, vec3);
vec3 FallbackIrradiance(vec3);
vec3 FallbackSky(vec3);
vec3 FresnelSchlick(float, vec3);
vec3 FresnelSchlickRoughness(float, vec3, float);
vec3 GetNormalFromTexture(sampler2D, vec2, vec3, vec3);
vec3 PointLightContribution(PointLight, vec3, vec3, vec3, float, float, vec3, vec3);
vec3 SpotLightContribution(SpotLight, vec3, vec3, vec3, float, float, vec3, vec3);
void main() {
        bool useAlbedoTexture = (v_textureMask & 0x1) != 0;
        bool useNormalTexture = (v_textureMask & 0x2) != 0;
        bool useMetalnessTexture = (v_textureMask & 0x4) != 0;
        bool useOcclusionTexture = (v_textureMask & 0x8) != 0;
        bool useRoughnessTexture = (v_textureMask & 0x10) != 0;
        bool useSpecularTexture = (v_textureMask & 0x20) != 0;
        bool useEmissiveTexture = (v_textureMask & 0x40) != 0;
        vec3 N = (useNormalTexture) ? GetNormalFromTexture(u_material.normal, v_texCoords, v_normal, v_tangent) : v_normal;
        vec4 A = (useAlbedoTexture) ? texture(u_material.albedo, v_texCoords) : v_albedo;
        vec4 S = (useSpecularTexture) ? texture(u_material.specular, v_texCoords) : v_specular;
        vec4 E = (useEmissiveTexture) ? texture(u_material.emissive, v_texCoords) : v_emissive;
        float O = (useOcclusionTexture) ? texture(u_material.occlusion, v_texCoords).r : v_occlusion;
        float R = (useRoughnessTexture) ? texture(u_material.roughness, v_texCoords).g : v_roughness;
        float M = (useMetalnessTexture) ? texture(u_material.metalness, v_texCoords).b : v_metalness;
        vec3 V = normalize(u_viewPos - v_position);
        vec3 reflectDir = reflect(-V, N);
        vec3 F0 = vec3(0.04) * S.rgb;
        F0 = mix(F0, A.rgb, vec3(M));
        vec3 Lo = vec3(0.0);
        vec3 ambient;
        if (u_hasSkybox) {
                vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, R);
                vec3 kS = F;
                vec3 kD = 1.0 - kS;
                kD *= 1.0 - vec3(M);
                vec3 irradiance = u_hasIrradianceMap ? texture(u_irradianceMap, N).rgb : FallbackIrradiance(N);
                vec3 diffuse = irradiance * A.rgb;
                vec3 prefilteredColor = u_hasPrefilterMap ? textureLod(u_prefilterMap, reflectDir, R * MAX_REFLECTION_LOD).rgb : FallbackSky(reflectDir);
                vec2 brdf = u_hasBRDF ? texture(u_brdfLUT, vec2(max(dot(N, V), 0.0), R)).rg : FallbackBRDF(max(dot(N, V), 0.0), R);
                vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
                ambient = (kD * diffuse + specular) * O;
        } else if (u_hasDirLight)
                ambient = u_dirLight.ambient * A.rgb * O;
        else
                ambient = vec3(0.03) * A.rgb * O;
        if (u_hasDirLight) {
                float dirShadow = ShadowAmount(u_shadowMap, -u_dirLight.direction, v_positionL, N);
                Lo += (1.0 - dirShadow) * DirLightContribution(u_dirLight, F0, A.rgb, N, M, R, V);
        }
        for (uint i = 0u; i < min(u_pointCount, MAX_POINT_LIGHTS); ++i)
                Lo += PointLightContribution(u_pointLights[i], F0, A.rgb, N, M, R, V, v_position);
        for (uint i = 0u; i < min(u_spotCount, MAX_SPOT_LIGHTS); ++i) {
                vec3 spotLightDir = normalize(u_spotLights[i].position - v_position);
                float spotShadow = SpotShadowAmount(u_spotShadowMap, float(i), u_spotLightViewProj[i], v_position, spotLightDir, N);
                Lo += (1.0 - spotShadow) * SpotLightContribution(u_spotLights[i], F0, A.rgb, N, M, R, V, v_position);
        }
        color = vec4(ambient + Lo + E.rgb, A.a);
        // NOTE: alpha is forced to 1.0 (not packed with a 4th ID byte) because an ID byte of 0 in alpha would blend this write away entirely
        entityColor = vec4(float((v_entityId >> 0) & 0xFFu) / 255.0, float((v_entityId >> 8) & 0xFFu) / 255.0, float((v_entityId >> 16) & 0xFFu) / 255.0, 1.0);
}
vec3 DirLightContribution(DirLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V) {
        vec3 L = normalize(-light.direction);
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
        vec3 diffuse = (A / PI) * light.diffuse * light.intensity;
        vec3 specular = numerator / denominator;
        return (kD * diffuse + specular * light.specular * light.intensity) * NdotL;
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
vec2 FallbackBRDF(float NdotV, float roughness) {
        return vec2(NdotV, 1.0 - roughness);
}
vec3 FallbackSky(vec3 dir) {
        float t = clamp(normalize(dir).y * 0.5 + 0.5, 0.0, 1.0);
        vec3 horizon = vec3(0.6, 0.7, 0.9);
        vec3 zenith = vec3(0.0, 0.1, 0.4);
        return mix(horizon, zenith, t);
}
vec3 FallbackIrradiance(vec3 dir) {
        return FallbackSky(dir) / PI;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float R) {
        return F0 + (max(vec3(1.0 - R), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
float ShadowCompare(vec3 posScreen, float mapDepth, vec3 N, vec3 lightDir) {
        float fragDepth = posScreen.z;
        float bias = max(0.05 * (1.0 - dot(N, lightDir)), 0.005);
        return fragDepth - bias > mapDepth ? 1.0 : 0.0;
}
float ShadowAmount(sampler2D shadowMap, vec3 lightDir, vec4 posClip, vec3 N) {
        vec3 posScreen = posClip.xyz / posClip.w;
        posScreen = posScreen * 0.5 + 0.5;
        float mapDepth = texture(shadowMap, posScreen.xy).r;
        return ShadowCompare(posScreen, mapDepth, N, lightDir);
}
float SpotShadowAmount(sampler2DArray shadowMap, float layer, mat4 viewProj, vec3 worldPos, vec3 lightDir, vec3 N) {
        vec4 posClip = viewProj * vec4(worldPos, 1.0);
        vec3 posScreen = posClip.xyz / posClip.w;
        posScreen = posScreen * 0.5 + 0.5;
        float mapDepth = texture(shadowMap, vec3(posScreen.xy, layer)).r;
        return ShadowCompare(posScreen, mapDepth, N, lightDir);
}
vec3 GetNormalFromTexture(sampler2D normalMap, vec2 texCoords, vec3 normal, vec3 tangent) {
        vec3 tangentNormal = texture(normalMap, texCoords).xyz * 2.0 - 1.0;
        vec3 N = normalize(normal);
        vec3 T = normalize(tangent);
        vec3 B = normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * tangentNormal);
}
vec3 PointLightContribution(PointLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, vec3 fragPos) {
        vec3 L = normalize(light.position - fragPos);
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
        float distance = length(light.position - fragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        vec3 diffuse = kD * A * light.diffuse * light.intensity * attenuation / PI;
        vec3 specular = light.specular * light.intensity * attenuation * numerator / denominator;
        return (diffuse + specular) * NdotL;
}
vec3 SpotLightContribution(SpotLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, vec3 fragPos) {
        vec3 L = normalize(light.position - fragPos);
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
        float distance = length(light.position - fragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        float theta = dot(L, normalize(-light.direction));
        float epsilon = light.innerCutoff - light.outerCutoff;
        float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
        attenuation *= intensity;
        vec3 diffuse = (A / PI) * light.diffuse * light.intensity * attenuation;
        vec3 specular = numerator / denominator;
        return (kD * diffuse + specular * light.specular * light.intensity * attenuation) * NdotL;
}
