#version 460 core
in vec2 texCoord;
out vec4 color;
uniform float threshold;
uniform sampler2D image;
void main() {
        color = texture(image, texCoord);
        float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > threshold)
                color = vec4(color.rgb, 1.0);
        else
                color = vec4(0.0, 0.0, 0.0, 1.0);
}
