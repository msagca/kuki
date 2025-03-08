#version 460 core
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
out vec3 texCoord;
uniform mat4 projection;
uniform mat4 view;
void main() {
    texCoord = i_position;
    vec4 pos = projection * view * vec4(i_position, 1.0);
    gl_Position = pos.xyww;
}
