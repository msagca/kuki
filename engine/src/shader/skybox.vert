#version 460 core
out vec3 texCoord;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
layout(std140, binding = 0) uniform i_cameraTransform {
        mat4 view;
        mat4 projection;
};
uniform mat4 model;
void main() {
        texCoord = i_position;
        mat4 viewNoTranslate = mat4(mat3(view));
        vec4 pos = projection * viewNoTranslate * model * vec4(i_position, 1.0);
        gl_Position = pos.xyww; // use w in place of z so that it's 1.0 (farthest away) after perspective division
}
