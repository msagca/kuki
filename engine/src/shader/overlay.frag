#version 460 core
in vec2 v_texCoords;
uniform sampler2D u_atlas;
uniform vec4 u_color;
out vec4 color;
void main() {
	// Only the alpha is read. The atlas is white everywhere and carries the glyph in its fourth
	// channel, so the colour is entirely the caller's and the texture only says where the ink is.
	color = vec4(u_color.rgb, u_color.a * texture(u_atlas, v_texCoords).a);
}
