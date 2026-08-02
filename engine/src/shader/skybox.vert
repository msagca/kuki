#version 460 core
out vec2 v_ndc;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tangent;
void main() {
        v_ndc = position.xy;
        gl_Position = vec4(position.xy, 1.0, 1.0);
}
