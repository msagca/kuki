#version 460 core
out vec3 localPos;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    localPos = i_position;
    gl_Position = projection * view * model * vec4(i_position, 1.0);
}
