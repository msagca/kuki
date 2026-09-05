#version 460 core
const float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);
const float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
in vec2 v_texCoords;
out vec4 color;
uniform bool u_horizontal;
uniform sampler2D u_image;
void main() {
        vec2 texelSize = 1.0 / textureSize(u_image, 0);
        vec3 result = texture(u_image, v_texCoords).rgb * weight[0];
        for (int i = 1; i < 3; ++i) {
                vec2 delta = u_horizontal ? vec2(texelSize.x * offset[i], 0.0) : vec2(0.0, texelSize.y * offset[i]);
                result += texture(u_image, v_texCoords + delta).rgb * weight[i];
                result += texture(u_image, v_texCoords - delta).rgb * weight[i];
        }
        color = vec4(result, 1.0);
}
