#version 460 core
out vec3 v_texCoords;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tangent;
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
};
uniform mat4 u_model;
void main() {
        v_texCoords = position;
        mat4 viewNoTranslate = mat4(mat3(view));
        vec4 pos = projection * viewNoTranslate * u_model * vec4(position, 1.0);
        gl_Position = pos.xyww; // use w in place of z so that it's 1.0 (farthest away) after perspective division
}
