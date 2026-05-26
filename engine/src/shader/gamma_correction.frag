#version 460 core
in vec2 v_texCoords;
out vec4 color;
uniform float u_gamma;
uniform sampler2D u_image;
void main() {
        vec4 colorLinear = texture(u_image, v_texCoords);
        color = vec4(pow(colorLinear.rgb, vec3(1.0 / u_gamma)), 1.0);
}
