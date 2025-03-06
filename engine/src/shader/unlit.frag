#version 460 core
in vec2 texCoord;
out vec4 color;
struct Material {
    sampler2D base;
};
uniform Material material;
void main() {
    color = vec4(texture(material.base, texCoord).rgb, 1.0);
}
