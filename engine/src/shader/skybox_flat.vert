#version 460 core
out vec2 texCoord;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec3 i_tangent;
void main() {
    texCoord = i_texCoord;
    gl_Position = vec4(i_position, 1.0);
}
