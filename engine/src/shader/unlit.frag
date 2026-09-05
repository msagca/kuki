#version 460 core
flat in int v_textureMask;
flat in uint v_entityId;
in vec2 v_texCoords;
in vec4 v_baseColor;
out vec4 color;
layout(location = 1) out vec4 entityColor;
struct Material {
        sampler2D base;
};
uniform Material u_material;
void main() {
        bool useBaseTexture = (v_textureMask & 0x1) != 0;
        color = useBaseTexture ? vec4(texture(u_material.base, v_texCoords)) : v_baseColor;
        entityColor = vec4(float((v_entityId >> 0) & 0xFFu) / 255.0, float((v_entityId >> 8) & 0xFFu) / 255.0, float((v_entityId >> 16) & 0xFFu) / 255.0, 1.0);
}
