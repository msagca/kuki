#version 330 core
out vec4 color;
uniform vec3 wireColor;
void main() {
    color = vec4(wireColor, 1.0);
}
