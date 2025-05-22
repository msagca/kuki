#version 460 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D hdrTexture;
uniform float exposure;
uniform float gamma;
void main() {
    vec4 hdrColor = texture(hdrTexture, texCoord);
    vec4 toneMapped = vec4(vec3(1.0) - exp(-hdrColor.rgb * exposure), hdrColor.a);
    vec4 gammaCorrected = pow(toneMapped, vec4(1.0 / gamma));
    color = gammaCorrected;
}
