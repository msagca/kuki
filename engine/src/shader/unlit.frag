#version 460 core
in vec2 texCoord;
out vec4 color;
struct Material {
    sampler2D base;
};
struct Fallback {
    vec4 base;
};
uniform Material material;
uniform Fallback fallback;
uniform bool useBaseTexture;
void main() {
    color = (useBaseTexture) ? vec4(texture(material.base, texCoord).rgb, 1.0) : fallback.base;
}
