#version 460 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec4 model0;
layout(location = 5) in vec4 model1;
layout(location = 6) in vec4 model2;
layout(location = 7) in vec4 model3;
layout(std140, binding = 0) uniform u_cameraTransform {
        mat4 view;
        mat4 projection;
};
void main() {
        mat4 model = mat4(model0, model1, model2, model3);
        gl_Position = projection * view * model * vec4(position, 1.0);
}
