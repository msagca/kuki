#version 460 core
out vec3 texCoord;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoord = i_position;
    vec4 pos = projection * view * model * vec4(i_position, 1.0);
    gl_Position = pos.xyww; // use w in place of z so that it's 1.0 (farthest away) after perspective division
}
