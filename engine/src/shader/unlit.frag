#version 460 core
flat in int v_textureMask;
in vec2 v_texCoords;
in vec4 v_baseColor;
out vec4 color;
struct Material {
        sampler2D base;
};
uniform Material u_material;
void main() {
        bool useBaseTexture = (v_textureMask & 0x1) != 0;
        color = useBaseTexture ? vec4(texture(u_material.base, v_texCoords)) : v_baseColor;
}
