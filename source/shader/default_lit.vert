#version 330 core
out vec3 position;
out vec3 normal;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    vec4 worldPosition = model * vec4(i_position, 1.0);
    position = vec3(worldPosition);
    normal = mat3(transpose(inverse(model))) * i_normal;
    gl_Position = projection * view * worldPosition;
}
