#version 460 core
in vec2 texCoord;
in vec4 baseColor;
flat in int textureMask;
out vec4 color;
struct Material {
        sampler2D base;
};
uniform Material material;
void main() {
        bool useBaseTexture = (textureMask & 0x1) != 0;
        color = (useBaseTexture) ? vec4(texture(material.base, texCoord).rgb, 1.0) : baseColor;
}
