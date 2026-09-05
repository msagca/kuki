#version 460 core
in vec2 v_texCoords;
out vec4 color;
uniform sampler2D u_imageBright;
uniform sampler2D u_image;
uniform float u_intensity;
void main() {
        vec3 base = texture(u_image, v_texCoords).rgb;
        vec3 bloom = texture(u_imageBright, v_texCoords).rgb;
        // Stays in radiance. The tone mapping pass owns the curve now, and compressing here as
        // well would run the image through two of them, the first with no exposure applied and
        // no way to choose it.
        color.rgb = base + bloom * u_intensity;
        color.a = 1.0;
}
