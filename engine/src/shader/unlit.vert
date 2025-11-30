#version 460 core
out vec2 texCoord;
out vec4 baseColor;
flat out int textureMask;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
layout(location = 4) in vec4 i_model0;
layout(location = 5) in vec4 i_model1;
layout(location = 6) in vec4 i_model2;
layout(location = 7) in vec4 i_model3;
layout(location = 8) in vec4 i_baseColor;
layout(location = 9) in int i_textureMask;
layout(std140, binding = 0) uniform i_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        mat4 model = mat4(i_model0, i_model1, i_model2, i_model3);
        baseColor = i_baseColor;
        texCoord = i_texCoord;
        textureMask = i_textureMask;
        gl_Position = projection * view * model * vec4(i_position, 1.0);
}
