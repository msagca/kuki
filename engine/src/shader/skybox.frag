#version 460 core
in vec3 texCoord;
out vec4 color;
uniform bool useSkybox;
uniform samplerCube skybox;
void main() {
        if (useSkybox)
                color = texture(skybox, texCoord);
        else { // fallback to gradient
                float t = clamp(normalize(texCoord).y * 0.5 + 0.5, 0.0, 1.0);
                vec3 horizon = vec3(0.6, 0.7, 0.9);
                vec3 zenith = vec3(0.0, 0.1, 0.4);
                vec3 gradient = mix(horizon, zenith, t);
                color = vec4(gradient, 1.0);
        }
}
