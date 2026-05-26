#version 460 core
const vec4 COLOR_GRAY = vec4(0.1, 0.1, 0.1, 1.0);
in vec3 v_texCoords;
out vec4 color;
uniform bool u_useSkybox;
uniform bool u_useGradient;
uniform samplerCube u_skybox;
void main() {
        if (u_useSkybox)
                color = texture(u_skybox, v_texCoords);
        else if (u_useGradient) {
                float t = clamp(normalize(v_texCoords).y * 0.5 + 0.5, 0.0, 1.0);
                vec3 horizon = vec3(0.6, 0.7, 0.9);
                vec3 zenith = vec3(0.0, 0.1, 0.4);
                vec3 gradient = mix(horizon, zenith, t);
                color = vec4(gradient, 1.0);
        } else
                color = COLOR_GRAY;
}
