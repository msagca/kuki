#version 460 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D hdrTexture;
uniform float exposure;
uniform float gamma;
void main() {
    vec3 hdrColor = texture(hdrTexture, texCoord).rgb;
    vec3 toneMapped = vec3(1.0) - exp(-hdrColor * exposure);
    vec3 gammaCorrected = pow(toneMapped, vec3(1.0 / gamma));
    color = vec4(gammaCorrected, 1.0);
}
