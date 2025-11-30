#version 460 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D hdrImage;
uniform sampler2D bloomImage;
uniform float exposure;
uniform float gamma;
void main() {
        vec4 sceneColor = texture(hdrImage, texCoord);
        vec4 bloomColor = texture(bloomImage, texCoord);
        sceneColor += bloomColor;
        vec4 toneMapped = vec4(vec3(1.0) - exp(-sceneColor.rgb * exposure), sceneColor.a);
        vec4 gammaCorrected = pow(toneMapped, vec4(1.0 / gamma));
        color = gammaCorrected;
}
