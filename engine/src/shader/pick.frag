#version 460 core
// One texel of the multisampled entity id buffer, read without resolving it.
//
// A blit out of a multisampled buffer resolves, and a resolve averages the samples it covers -- so
// the average of two ids is a third id that belongs to neither, and a click on the silhouette of an
// object answered with an entity that is not in the scene. Fetching the samples means every value
// considered here is one a triangle actually wrote.
out vec4 color;
uniform sampler2DMS u_idImage;
uniform int u_coordX;
uniform int u_coordY;
uniform int u_sampleCount;
const uint ENTITY_ID_INVALID = 0xFFFFFFu;
uint DecodeId(vec4 encoded) {
        return uint(encoded.r * 255.0 + 0.5) | (uint(encoded.g * 255.0 + 0.5) << 8) | (uint(encoded.b * 255.0 + 0.5) << 16);
}
void main() {
        ivec2 coord = ivec2(u_coordX, u_coordY);
        // Sample 0 first, because that is the one the outline pass draws from: a click and the
        // highlight it produces should agree about what is under the cursor. The rest are consulted
        // only when sample 0 is background, which lets a click land on a piece from a pixel the
        // piece covers by a quarter -- generous at the silhouette, never generous towards nothing.
        color = texelFetch(u_idImage, coord, 0);
        for (int i = 1; i < u_sampleCount && DecodeId(color) == ENTITY_ID_INVALID; ++i)
                color = texelFetch(u_idImage, coord, i);
}
