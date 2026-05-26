#version 460 core
in vec2 v_texCoords;
out vec4 color;
uniform sampler2D u_imageBright;
uniform sampler2D u_image;
uniform float u_intensity;
void main() {
        vec3 base = texture(u_image, v_texCoords).rgb;
        vec3 bloom = texture(u_imageBright, v_texCoords).rgb;
        color.rgb = base + bloom * u_intensity;
        // Reinhard tone mapping
        color.rgb = color.rgb / (color.rgb + vec3(1.0));
        color.a = 1.0;
}
