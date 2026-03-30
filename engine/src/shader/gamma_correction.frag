#version 460 core
in vec2 texCoord;
out vec4 color;
uniform float gamma;
uniform sampler2D image;
void main() {
        vec4 colorLinear = texture(image, texCoord);
        color = vec4(pow(colorLinear.rgb, vec3(1.0 / gamma)), 1.0);
}
