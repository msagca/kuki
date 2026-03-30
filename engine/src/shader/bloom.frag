#version 460 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D imageBright;
uniform sampler2D image;
uniform float intensity;
void main() {
        vec3 base = texture(image, texCoord).rgb;
        vec3 bloom = texture(imageBright, texCoord).rgb;
        color.rgb = base + bloom * intensity;
        // Reinhard tone mapping
        color.rgb = color.rgb / (color.rgb + vec3(1.0));
        color.a = 1.0;
}
