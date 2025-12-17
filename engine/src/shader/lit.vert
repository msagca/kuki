#version 460 core
flat out int textureMask;
out float metalness;
out float occlusion;
out float roughness;
out vec2 texCoord;
out vec3 normal;
out vec3 position;
out vec3 tangent;
out vec4 albedo;
out vec4 emissive;
out vec4 specular;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
layout(location = 4) in vec4 i_model0;
layout(location = 5) in vec4 i_model1;
layout(location = 6) in vec4 i_model2;
layout(location = 7) in vec4 i_model3;
layout(location = 8) in vec4 i_albedo;
layout(location = 9) in vec4 i_specular;
layout(location = 10) in vec4 i_emissive;
layout(location = 11) in float i_metalness;
layout(location = 12) in float i_occlusion;
layout(location = 13) in float i_roughness;
layout(location = 14) in int i_textureMask;
layout(std140, binding = 0) uniform i_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
        vec4 worldPosition = model * vec4(i_position, 1.0);
        position = vec3(worldPosition);
        normal = mat3(transpose(inverse(model))) * i_normal;
        texCoord = i_texCoord;
        tangent = i_tangent;
        albedo = i_albedo;
        specular = i_specular;
        emissive = i_emissive;
        metalness = i_metalness;
        occlusion = i_occlusion;
        roughness = i_roughness;
        textureMask = i_textureMask;
        gl_Position = projection * view * worldPosition;
}
