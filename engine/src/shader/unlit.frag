#version 460 core
flat in int textureMask;
in vec2 texCoord;
in vec4 baseColor;
out vec4 color;
struct Material {
        sampler2D base;
};
uniform Material material;
void main() {
        bool useBaseTexture = (textureMask & 0x1) != 0;
        color = useBaseTexture ? vec4(texture(material.base, texCoord).rgb, 1.0) : baseColor;
}
