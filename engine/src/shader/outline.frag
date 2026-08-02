#version 460 core
const uint MAX_SELECTED = 16;
in vec2 v_texCoords;
out vec4 color;
uniform float u_outlineThickness;
uniform sampler2D u_idImage;
uniform sampler2D u_image;
uniform uint u_selectedCount;
uniform uint u_selectedIds[MAX_SELECTED];
uniform vec2 u_texelSize;
uniform vec3 u_outlineColor;
uint DecodeId(vec4 idColor) {
        // NOTE: only the low 24 bits (RGB) carry an entity ID -- see `GLRenderer::PickEntity` for why alpha is never part of the encoding
        return uint(idColor.r * 255.0 + 0.5) | (uint(idColor.g * 255.0 + 0.5) << 8) | (uint(idColor.b * 255.0 + 0.5) << 16);
}
bool IsSelected(uint id) {
        for (uint i = 0u; i < u_selectedCount; ++i)
                if (u_selectedIds[i] == id)
                        return true;
        return false;
}
void main() {
        color = texture(u_image, v_texCoords);
        if (u_selectedCount == 0u)
                return;
        uint centerId = DecodeId(texture(u_idImage, v_texCoords));
        if (IsSelected(centerId))
                return;
        const int SAMPLE_COUNT = 8;
        vec2 offsets[SAMPLE_COUNT] = vec2[](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));
        for (int i = 0; i < SAMPLE_COUNT; ++i) {
                vec2 uv = v_texCoords + offsets[i] * u_texelSize * u_outlineThickness;
                uint neighborId = DecodeId(texture(u_idImage, uv));
                if (IsSelected(neighborId)) {
                        color = vec4(u_outlineColor, 1.0);
                        return;
                }
        }
}
