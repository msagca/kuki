#version 460 core
in vec2 texCoord;
out vec4 color;
uniform float exposure;
uniform float gamma;
uniform sampler2D image;
void main() {
        color = texture(image, texCoord);
        vec4 toneMapped = vec4(vec3(1.0) - exp(-color.rgb * exposure), color.a);
       vec3 gammaCorrectedRGB = pow(toneMapped.rgb, vec3(1.0 / gamma));
color = vec4(gammaCorrectedRGB, toneMapped.a);
}
