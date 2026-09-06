#version 460 core
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_texCoords;
uniform mat4 u_projection;
out vec2 v_texCoords;
void main() {
	v_texCoords = a_texCoords;
	gl_Position = u_projection * vec4(a_position, 1.0);
}
