#version 460 core
const uint ALPHA_MODE_OPAQUE = 0u;
const uint ALPHA_MODE_MASK = 1u;
const float EPSILON = 1.0e-6;
const float MAX_REFLECTION_LOD = 6.0;
const float PI = 3.14159265359;
const uint MAX_POINT_LIGHTS = KUKI_MAX_POINT_LIGHTS;
const uint MAX_SPOT_LIGHTS = KUKI_MAX_SPOT_LIGHTS;
// Taps the shadow filter takes to each side of the centre, so two is a five by five kernel and
// twenty-six shades between lit and shadowed. One tap gives two shades and a staircase for an edge;
// the eye picks out the steps of a three by three as banding, and stops being able to at five.
const int SHADOW_FILTER_EXTENT = 2;
const float SHADOW_FILTER_TAPS = float((2 * SHADOW_FILTER_EXTENT + 1) * (2 * SHADOW_FILTER_EXTENT + 1));
// What is left for the slope bias to not have to cover: depth quantisation in the map itself. It can
// be this small only because the gradient carries the part that scales with the angle to the light.
const float SHADOW_DEPTH_BIAS = 0.0002;
// How much of the map's depth range the receiver plane is allowed to claim its own depth crosses
// over the filter's footprint before that plane is thrown out. Past this it did not come from a
// single surface: a derivative quad straddling a crease holds two of them and solves for a plane
// through neither, and a surface the light grazes projects to so thin a sliver of the map that the
// solve divides by an area near zero. Both hand back a slope far larger than real geometry asks for.
const float SHADOW_SLOPE_LIMIT = 0.01;
// The step taken along the surface normal before a sample is projected into the light, measured in
// shadow map texels at that sample's distance from it.
//
// This is what carries the bias where the plane is thrown out, and it is why throwing it out is
// safe. It moves the sample out of its own surface in the map rather than along the depth axis, so
// there is nothing in it that has to be retuned when the projection moves, and at a concave corner
// it steps away from both faces at once -- which is where the plane solve is least trustworthy.
// Below about one texel it stops clearing the map's own sampling grid; much above two and contact
// shadows begin to detach from what casts them.
const float SHADOW_NORMAL_OFFSET = 1.5;
flat in int v_textureMask;
flat in uint v_entityId;
in float v_metalness;
in float v_occlusion;
in float v_roughness;
in vec2 v_texCoords;
in vec3 v_normal;
in vec3 v_position;
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
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
};
uniform DirLight u_dirLight;
uniform Material u_material;
uniform uint u_alphaMode;
uniform float u_alphaCutoff;
uniform vec3 u_attenuationColor;
uniform float u_attenuationDistance;
uniform float u_transmission;
uniform float u_thickness;
uniform float u_ior;
uniform sampler2D u_sceneColor;
uniform PointLight u_pointLights[MAX_POINT_LIGHTS];
uniform SpotLight u_spotLights[MAX_SPOT_LIGHTS];
uniform bool u_hasBRDF;
uniform bool u_hasDirLight;
uniform bool u_hasIrradianceMap;
uniform bool u_hasPrefilterMap;
uniform bool u_hasSkybox;
uniform sampler2D u_brdfLUT;
uniform float u_skyIntensity;
uniform float u_ambientFallback;
uniform sampler2D u_shadowMap;
uniform sampler2DArray u_spotShadowMap;
uniform mat4 u_spotLightViewProj[MAX_SPOT_LIGHTS];
uniform samplerCube u_irradianceMap;
uniform samplerCube u_prefilterMap;
uniform uint u_pointCount;
uniform uint u_spotCount;
uniform vec3 u_viewPos;
bool OutsideShadowMap(vec3);
float DistributionGGX(vec3, vec3, float);
float GeometrySchlickGGX(float, float);
float GeometrySmith(vec3, vec3, vec3, float);
float ShadowAmount(sampler2D, mat4, vec3, vec3, vec3);
float ShadowTexelWorldSize(mat4, vec3, vec2);
float SpotShadowAmount(sampler2DArray, float, mat4, vec3, vec3, vec3);
vec2 ShadowDepthGradient(vec3, vec2);
vec2 FallbackBRDF(float, float);
vec3 DirLightContribution(DirLight, vec3, vec3, vec3, float, float, vec3, float);
vec3 FallbackIrradiance(vec3);
vec3 FallbackSky(vec3);
vec3 FresnelSchlick(float, vec3);
vec3 FresnelSchlickRoughness(float, vec3, float);
vec3 GetNormalFromTexture(sampler2D, vec2, vec3, vec3);
vec3 PointLightContribution(PointLight, vec3, vec3, vec3, float, float, vec3, vec3, float);
vec3 SpotLightContribution(SpotLight, vec3, vec3, vec3, float, float, vec3, vec3, float);
vec3 SampleTransmission(vec3, vec3, vec3, vec3);
vec3 ShadowSamplePosition(mat4, vec3, vec3, vec3, vec2);
vec3 VolumeAttenuation(float);
void main() {
        bool useAlbedoTexture = (v_textureMask & 0x1) != 0;
        bool useNormalTexture = (v_textureMask & 0x2) != 0;
        bool useMetalnessTexture = (v_textureMask & 0x4) != 0;
        bool useOcclusionTexture = (v_textureMask & 0x8) != 0;
        bool useRoughnessTexture = (v_textureMask & 0x10) != 0;
        bool useSpecularTexture = (v_textureMask & 0x20) != 0;
        bool useEmissiveTexture = (v_textureMask & 0x40) != 0;
        // Kept apart from N because the shadow lookups step along the interpolated surface normal,
        // not the one a normal map bends. The map holds no record of that detail, and a step whose
        // length jumps from pixel to pixel is the one thing the receiver plane solve cannot survive.
        vec3 GN = normalize(v_normal);
        vec3 N = (useNormalTexture) ? GetNormalFromTexture(u_material.normal, v_texCoords, GN, v_tangent) : GN;
        vec4 A = (useAlbedoTexture) ? texture(u_material.albedo, v_texCoords) * v_albedo : v_albedo;
        if (u_alphaMode == ALPHA_MODE_MASK && A.a < u_alphaCutoff)
                discard;
        float alpha = (u_alphaMode == ALPHA_MODE_OPAQUE) ? 1.0 : A.a;
        vec4 S = (useSpecularTexture) ? texture(u_material.specular, v_texCoords) : v_specular;
        vec4 E = (useEmissiveTexture) ? texture(u_material.emissive, v_texCoords) : v_emissive;
        float O = (useOcclusionTexture) ? texture(u_material.occlusion, v_texCoords).r : v_occlusion;
        float R = (useRoughnessTexture) ? texture(u_material.roughness, v_texCoords).g : v_roughness;
        float M = (useMetalnessTexture) ? texture(u_material.metalness, v_texCoords).b : v_metalness;
        vec3 V = normalize(u_viewPos - v_position);
        vec3 reflectDir = reflect(-V, N);
        float ior = max(u_ior, 1.0);
        float dielectric = pow((ior - 1.0) / (ior + 1.0), 2.0);
        vec3 F0 = vec3(dielectric) * S.rgb;
        F0 = mix(F0, A.rgb, vec3(M));
        float transmissionWeight = clamp(u_transmission, 0.0, 1.0) * (1.0 - M);
        float diffuseScale = 1.0 - transmissionWeight;
        vec3 ambientF = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, R);
        vec3 ambientKD = (1.0 - ambientF) * (1.0 - vec3(M));
        vec3 transmitted = transmissionWeight > 0.0 ? ambientKD * transmissionWeight * SampleTransmission(v_position, N, V, A.rgb) : vec3(0.0);
        vec3 Lo = vec3(0.0);
        vec3 ambient;
        if (u_hasSkybox) {
                vec3 kD = ambientKD * diffuseScale;
                vec3 irradiance = (u_hasIrradianceMap ? texture(u_irradianceMap, N).rgb : FallbackIrradiance(N)) * u_skyIntensity;
                vec3 diffuse = irradiance * A.rgb;
                vec3 prefilteredColor = u_hasPrefilterMap ? textureLod(u_prefilterMap, reflectDir, R * MAX_REFLECTION_LOD).rgb : FallbackSky(reflectDir);
                vec2 brdf = u_hasBRDF ? texture(u_brdfLUT, vec2(max(dot(N, V), 0.0), R)).rg : FallbackBRDF(max(dot(N, V), 0.0), R);
                vec3 specular = prefilteredColor * (ambientF * brdf.x + brdf.y);
                ambient = (kD * diffuse + specular) * O;
        } else if (u_hasDirLight)
                ambient = u_dirLight.ambient * diffuseScale * A.rgb * O;
        else
                ambient = vec3(u_ambientFallback) * diffuseScale * A.rgb * O;
        if (u_hasDirLight) {
                mat4 lightViewProj = u_dirLight.projection * u_dirLight.view;
                float dirShadow = ShadowAmount(u_shadowMap, lightViewProj, v_position, GN, normalize(-u_dirLight.direction));
                Lo += (1.0 - dirShadow) * DirLightContribution(u_dirLight, F0, A.rgb, N, M, R, V, diffuseScale);
        }
        for (uint i = 0u; i < min(u_pointCount, MAX_POINT_LIGHTS); ++i)
                Lo += PointLightContribution(u_pointLights[i], F0, A.rgb, N, M, R, V, v_position, diffuseScale);
        for (uint i = 0u; i < min(u_spotCount, MAX_SPOT_LIGHTS); ++i) {
                float spotShadow = SpotShadowAmount(u_spotShadowMap, float(i), u_spotLightViewProj[i], v_position, GN, normalize(u_spotLights[i].position - v_position));
                Lo += (1.0 - spotShadow) * SpotLightContribution(u_spotLights[i], F0, A.rgb, N, M, R, V, v_position, diffuseScale);
        }
        color = vec4(ambient + Lo + transmitted + E.rgb, alpha);
        entityColor = vec4(float((v_entityId >> 0) & 0xFFu) / 255.0, float((v_entityId >> 8) & 0xFFu) / 255.0, float((v_entityId >> 16) & 0xFFu) / 255.0, 1.0);
}
vec3 DirLightContribution(DirLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, float diffuseScale) {
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
        kD *= (1.0 - vec3(M)) * diffuseScale;
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
// How the receiver's own depth changes per unit step across the shadow map, recovered from the
// screen-space derivatives of its shadow-map coordinate.
//
// A shadow map holds one depth per texel and the filters below read twenty-five of them, but a
// surface at any angle to the light sits at a different depth under every one. Comparing them all
// against the depth at the centre tap is what makes a sloped surface shadow itself in stripes. The
// usual answer pushes the comparison back by a constant scaled by the angle, which trades the
// stripes for light leaking out from under the surface, and has to be retuned whenever the
// projection moves, because that constant lives in a post-projection depth that is not linear in
// distance -- the reason the old 0.05 * (1 - N.L) erased the shadows on a spot-lit wall entirely
// while leaving the floor's intact. Solving for the plane the receiver actually lies on instead lets
// each tap be compared against the receiver's depth *at that tap*. It is exact for a flat surface at
// any angle to the light, and there is nothing in it to tune -- where it holds at all.
vec2 ShadowDepthGradient(vec3 posScreen, vec2 texel) {
        vec3 dx = dFdx(posScreen);
        vec3 dy = dFdy(posScreen);
        float area = dx.x * dy.y - dx.y * dy.x;
        // A relative floor rather than an absolute one. How large this area is in the first place is
        // set by how much of the map a pixel covers, which moves with the distance to the light and
        // with the map's resolution, so a fixed epsilon either never fires or fires across a whole
        // surface. What it has to catch is the solve going singular -- the quad's two steps across
        // the map running parallel, which is what a surface the light grazes does to them.
        if (abs(area) < EPSILON * max(length(dx.xy) * length(dy.xy), EPSILON))
                return vec2(0.0);
        vec2 gradient = vec2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / area;
        // The largest bias the furthest tap will ask this gradient for. Over the limit the plane is
        // dropped rather than clamped to it: a clamped slope keeps the sign it was given, and on the
        // taps where that sign is negative it pulls the comparison in front of the occluder and
        // calls the tap lit. That is the bright line through a shadow along a wall-to-ceiling
        // crease, and it moves with the camera because which derivative quads straddle the crease is
        // a screen-space fact. The normal offset is what holds the bias up once the plane is gone.
        if (float(SHADOW_FILTER_EXTENT) * dot(texel, abs(gradient)) > SHADOW_SLOPE_LIMIT)
                return vec2(0.0);
        return gradient;
}
// The world-space width of one shadow map texel at a point, read off the light's own matrix so that
// nothing has to be uploaded beside it. A world step reaches clip space through the matrix's first
// two rows and normalised device space through those divided by w, so the world distance one texel
// spans is its share of the two device units the map covers, scaled back up by w. For the
// directional light's orthographic matrix w is one and this is constant across the scene; for a spot
// it grows with distance from the light, which is what a perspective shadow map does to its texels.
float ShadowTexelWorldSize(mat4 viewProj, vec3 worldPos, vec2 size) {
        float w = max((viewProj * vec4(worldPos, 1.0)).w, EPSILON);
        vec2 clipPerWorld = vec2(length(vec3(viewProj[0].x, viewProj[1].x, viewProj[2].x)), length(vec3(viewProj[0].y, viewProj[1].y, viewProj[2].y)));
        return max(2.0 * w / max(size.x * clipPerWorld.x, EPSILON), 2.0 * w / max(size.y * clipPerWorld.y, EPSILON));
}
// Where the map is asked about, which is not quite where the surface is.
//
// The map holds one depth per texel, and the surface under a texel spans a range of depths, so a
// point taken on the surface itself sits behind its own recorded depth over half of every texel it
// falls in -- the surface shadowing itself. Stepping out along the normal first lifts the point
// clear of that range. The step has to be longer the more depth a texel holds, which is the sine of
// the angle between surface and light: nothing head on, most at a graze.
vec3 ShadowSamplePosition(mat4 viewProj, vec3 worldPos, vec3 N, vec3 L, vec2 size) {
        float NdotL = clamp(dot(N, L), 0.0, 1.0);
        float sine = sqrt(clamp(1.0 - NdotL * NdotL, 0.0, 1.0));
        return worldPos + N * (SHADOW_NORMAL_OFFSET * sine * ShadowTexelWorldSize(viewProj, worldPos, size));
}
// A fragment the light never rendered has no depth to compare against, and the shadow targets clamp
// to edge, so sampling one anyway returns whatever sits on the border of the map and applies it to
// everything beyond. Out of range is unlit by this light, not shadowed by it, so the lookup is
// skipped rather than clamped. `z` is tested along with the plane, since a fragment past the far
// plane projects inside the map and is just as absent from it.
bool OutsideShadowMap(vec3 posScreen) {
        return posScreen.z < 0.0 || posScreen.z > 1.0 || any(lessThan(posScreen.xy, vec2(0.0))) || any(greaterThan(posScreen.xy, vec2(1.0)));
}
float ShadowAmount(sampler2D shadowMap, mat4 viewProj, vec3 worldPos, vec3 N, vec3 L) {
        vec2 size = vec2(textureSize(shadowMap, 0));
        vec2 texel = 1.0 / size;
        vec4 posClip = viewProj * vec4(ShadowSamplePosition(viewProj, worldPos, N, L, size), 1.0);
        vec3 posScreen = posClip.xyz / posClip.w;
        posScreen = posScreen * 0.5 + 0.5;
        // taken before anything returns, since a derivative is only meaningful where the whole quad
        // took the same path to it
        vec2 gradient = ShadowDepthGradient(posScreen, texel);
        if (posClip.w <= 0.0 || OutsideShadowMap(posScreen))
                return 0.0;
        float shadow = 0.0;
        for (int y = -SHADOW_FILTER_EXTENT; y <= SHADOW_FILTER_EXTENT; ++y)
                for (int x = -SHADOW_FILTER_EXTENT; x <= SHADOW_FILTER_EXTENT; ++x) {
                        vec2 offset = vec2(x, y) * texel;
                        float slope = dot(offset, gradient);
                        float mapDepth = textureLod(shadowMap, posScreen.xy + offset, 0.0).r;
                        shadow += posScreen.z + slope - SHADOW_DEPTH_BIAS > mapDepth ? 1.0 : 0.0;
                }
        return shadow / SHADOW_FILTER_TAPS;
}
float SpotShadowAmount(sampler2DArray shadowMap, float layer, mat4 viewProj, vec3 worldPos, vec3 N, vec3 L) {
        vec2 size = vec2(textureSize(shadowMap, 0).xy);
        vec2 texel = 1.0 / size;
        vec4 posClip = viewProj * vec4(ShadowSamplePosition(viewProj, worldPos, N, L, size), 1.0);
        vec3 posScreen = posClip.xyz / posClip.w;
        posScreen = posScreen * 0.5 + 0.5;
        vec2 gradient = ShadowDepthGradient(posScreen, texel);
        // A spot's projection is perspective, so a fragment behind the light divides by a negative
        // and lands back inside the map mirrored through the origin. The directional light's
        // orthographic projection cannot do this, but it costs nothing to guard both the same way.
        if (posClip.w <= 0.0 || OutsideShadowMap(posScreen))
                return 0.0;
        float shadow = 0.0;
        for (int y = -SHADOW_FILTER_EXTENT; y <= SHADOW_FILTER_EXTENT; ++y)
                for (int x = -SHADOW_FILTER_EXTENT; x <= SHADOW_FILTER_EXTENT; ++x) {
                        vec2 offset = vec2(x, y) * texel;
                        float slope = dot(offset, gradient);
                        float mapDepth = textureLod(shadowMap, vec3(posScreen.xy + offset, layer), 0.0).r;
                        shadow += posScreen.z + slope - SHADOW_DEPTH_BIAS > mapDepth ? 1.0 : 0.0;
                }
        return shadow / SHADOW_FILTER_TAPS;
}
vec3 VolumeAttenuation(float distance) {
        if (u_attenuationDistance <= 0.0 || distance <= 0.0)
                return vec3(1.0);
        vec3 coefficient = -log(max(u_attenuationColor, vec3(EPSILON))) / u_attenuationDistance;
        return exp(-coefficient * distance);
}
vec3 SampleTransmission(vec3 worldPos, vec3 N, vec3 V, vec3 A) {
        float ior = max(u_ior, 1.0);
        float thickness = max(u_thickness, 0.0);
        vec3 direction = refract(-V, N, 1.0 / ior);
        if (dot(direction, direction) < EPSILON)
                direction = -V;
        vec4 clipPos = projection * view * vec4(worldPos + direction * thickness, 1.0);
        if (clipPos.w <= EPSILON)
                return vec3(0.0);
        vec2 uv = clamp(clipPos.xy / clipPos.w * 0.5 + 0.5, 0.0, 1.0);
        vec3 background = textureLod(u_sceneColor, uv, 0.0).rgb;
        return background * VolumeAttenuation(thickness) * A;
}
// Only red and green are read, and z is rebuilt from them. Normal maps are block-compressed to BC5,
// which keeps two channels and leaves blue at zero, and a unit tangent-space normal makes the third
// component redundant anyway, so this reads an uncompressed map identically.
vec3 GetNormalFromTexture(sampler2D normalMap, vec2 texCoords, vec3 normal, vec3 tangent) {
        vec2 tangentXY = texture(normalMap, texCoords).xy * 2.0 - 1.0;
        vec3 tangentNormal = vec3(tangentXY, sqrt(max(0.0, 1.0 - dot(tangentXY, tangentXY))));
        vec3 N = normalize(normal);
        vec3 T = normalize(tangent);
        vec3 B = normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * tangentNormal);
}
vec3 PointLightContribution(PointLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, vec3 fragPos, float diffuseScale) {
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
        kD *= (1.0 - vec3(M)) * diffuseScale;
        float distance = length(light.position - fragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        vec3 diffuse = kD * A * light.diffuse * light.intensity * attenuation / PI;
        vec3 specular = light.specular * light.intensity * attenuation * numerator / denominator;
        return (diffuse + specular) * NdotL;
}
vec3 SpotLightContribution(SpotLight light, vec3 F0, vec3 A, vec3 N, float M, float R, vec3 V, vec3 fragPos, float diffuseScale) {
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
        kD *= (1.0 - vec3(M)) * diffuseScale;
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
