#version 460 core
in vec2 texCoord;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 brightColor;
uniform sampler2D image;
void main() {
        color = texture(image, texCoord);
        float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > 1.0)
                brightColor = vec4(color.rgb, 1.0);
        else
                brightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
