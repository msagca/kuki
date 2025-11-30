#version 460 core
out vec3 position;
out vec3 normal;
out vec2 texCoord;
out vec3 tangent;
out vec4 albedo;
out vec4 specular;
out vec4 emissive;
out float metalness;
out float occlusion;
out float roughness;
flat out int textureMask;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
layout(location = 4) in uvec4 i_boneIds;
layout(location = 5) in vec4 i_boneWeights;
layout(location = 6) in vec4 i_albedo;
layout(location = 7) in vec4 i_specular;
layout(location = 8) in vec4 i_emissive;
layout(location = 9) in float i_metalness;
layout(location = 10) in float i_occlusion;
layout(location = 11) in float i_roughness;
layout(location = 12) in int i_textureMask;
layout(std430, binding = 0) readonly buffer i_boneTransforms {
        mat4 boneTransforms[];
};
layout(std140, binding = 0) uniform i_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        mat4 model = mat4(1.0);
        bvec4 bonesValid = lessThan(i_boneIds, uvec4(boneTransforms.length()));
        if (all(bonesValid))
                model = i_boneWeights.x * boneTransforms[i_boneIds.x] + i_boneWeights.y * boneTransforms[i_boneIds.y] + i_boneWeights.z * boneTransforms[i_boneIds.z] + i_boneWeights.w * boneTransforms[i_boneIds.w];
        vec4 worldPosition = model * vec4(i_position, 1.0);
        position = vec3(worldPosition);
        normal = mat3(transpose(inverse(model))) * i_normal;
        texCoord = i_texCoord;
        tangent = mat3(model) * i_tangent.xyz;
        albedo = i_albedo;
        specular = i_specular;
        emissive = i_emissive;
        metalness = i_metalness;
        occlusion = i_occlusion;
        roughness = i_roughness;
        textureMask = i_textureMask;
        gl_Position = projection * view * worldPosition;
}
