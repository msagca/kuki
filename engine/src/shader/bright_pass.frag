#version 460 core
in vec2 v_texCoords;
out vec4 color;
uniform float u_threshold;
uniform sampler2D u_image;
void main() {
        color = texture(u_image, v_texCoords);
        float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > u_threshold)
                color = vec4(color.rgb, 1.0);
        else
                color = vec4(0.0, 0.0, 0.0, 1.0);
}
