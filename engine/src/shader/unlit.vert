#version 460 core
flat out int v_textureMask;
flat out uint v_entityId;
out vec2 v_texCoords;
out vec4 v_baseColor;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec4 model0;
layout(location = 5) in vec4 model1;
layout(location = 6) in vec4 model2;
layout(location = 7) in vec4 model3;
layout(location = 8) in vec4 baseColor;
layout(location = 9) in int textureMask;
layout(location = 15) in uint entityId;
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        mat4 model = mat4(model0, model1, model2, model3);
        v_baseColor = baseColor;
        v_texCoords = texCoords;
        v_textureMask = textureMask;
        v_entityId = entityId;
        gl_Position = projection * view * model * vec4(position, 1.0);
}
