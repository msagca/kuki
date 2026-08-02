#version 460 core
const vec4 COLOR_GRAY = vec4(0.1, 0.1, 0.1, 1.0);
in vec2 v_ndc;
out vec4 color;
layout(location = 1) out vec4 entityColor;
uniform bool u_useSkybox;
uniform bool u_useGradient;
uniform samplerCube u_skybox;
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        vec4 viewPos = inverse(projection) * vec4(v_ndc, 1.0, 1.0);
        vec3 direction = normalize(transpose(mat3(view)) * (viewPos.xyz / viewPos.w));
        if (u_useSkybox)
                color = texture(u_skybox, direction);
        else if (u_useGradient) {
                float t = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
                vec3 horizon = vec3(0.6, 0.7, 0.9);
                vec3 zenith = vec3(0.0, 0.1, 0.4);
                vec3 gradient = mix(horizon, zenith, t);
                color = vec4(gradient, 1.0);
        } else
                color = COLOR_GRAY;
        entityColor = vec4(1.0);
}
