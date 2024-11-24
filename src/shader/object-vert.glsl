#version 330 core
out vec3 fragPos;
out vec3 normal;
out vec2 texCoords;
layout(location = 0) in vec3 posIn;
layout(location = 1) in vec3 normIn;
layout(location = 2) in vec2 texCoordsIn;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    texCoords = texCoordsIn;
    fragPos = vec3(model * vec4(posIn, 1.0));
    normal = mat3(transpose(inverse(model))) * normIn;
    gl_Position = projection * view * model * vec4(posIn, 1.0);
}
