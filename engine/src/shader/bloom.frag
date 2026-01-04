#version 460 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D imageBright;
uniform sampler2D image;
void main() {
        color = texture(image, texCoord);
        color += texture(imageBright, texCoord);
}
