#version 460 core
out vec2 v_texCoords;
out vec3 v_worldPos;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 tangent;
uniform mat4 u_model;
uniform mat4 u_projection;
uniform mat4 u_view;
void main() {
        vec4 worldPos4 = u_model * vec4(position, 1.0);
        v_worldPos = vec3(worldPos4);
        v_texCoords = texCoords;
        gl_Position = u_projection * u_view * worldPos4;
}
