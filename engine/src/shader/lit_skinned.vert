#version 460 core
flat out int v_textureMask;
flat out uint v_entityId;
out float v_metalness;
out float v_occlusion;
out float v_roughness;
out vec2 v_texCoords;
out vec3 v_normal;
out vec3 v_position;
out vec4 v_positionL;
out vec3 v_tangent;
out vec4 v_albedo;
out vec4 v_emissive;
out vec4 v_specular;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 tangent;
layout(location = 4) in uvec4 boneIds;
layout(location = 5) in vec4 boneWeights;
layout(location = 8) in vec4 albedo;
layout(location = 9) in vec4 specular;
layout(location = 10) in vec4 emissive;
layout(location = 11) in float metalness;
layout(location = 12) in float occlusion;
layout(location = 13) in float roughness;
layout(location = 14) in int textureMask;
layout(location = 15) in uint entityId;
layout(std430, binding = 0) readonly buffer u_boneTransforms {
        mat4 boneTransforms[];
};
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
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
uniform DirLight u_dirLight;
void main() {
        mat4 model = mat4(1.0);
        bvec4 bonesValid = lessThan(boneIds, uvec4(boneTransforms.length()));
        if (all(bonesValid))
                model = boneWeights.x * boneTransforms[boneIds.x] + boneWeights.y * boneTransforms[boneIds.y] + boneWeights.z * boneTransforms[boneIds.z] + boneWeights.w * boneTransforms[boneIds.w];
        vec4 worldPosition = model * vec4(position, 1.0);
        v_position = vec3(worldPosition);
        v_normal = mat3(transpose(inverse(model))) * normal;
        v_texCoords = texCoords;
        v_tangent = mat3(model) * tangent.xyz;
        v_albedo = albedo;
        v_specular = specular;
        v_emissive = emissive;
        v_metalness = metalness;
        v_occlusion = occlusion;
        v_roughness = roughness;
        v_textureMask = textureMask;
        v_entityId = entityId;
        v_positionL = u_dirLight.projection * u_dirLight.view * worldPosition;
        gl_Position = projection * view * worldPosition;
}
