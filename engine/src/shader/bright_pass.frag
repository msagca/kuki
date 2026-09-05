#version 460 core
in vec2 v_texCoords;
out vec4 color;
uniform float u_exposure;
uniform float u_threshold;
uniform sampler2D u_image;
void main() {
        vec4 source = texture(u_image, v_texCoords);
        // Thresholded on the exposed image but extracted from the unexposed one, so that what
        // glows is decided in the brightness the viewer ends up seeing while the extracted
        // radiance stays in the space the rest of the chain works in. Testing before exposure
        // instead would hold the bloom still while the image around it brightened, which reads
        // as the effect coming loose from the scene.
        float brightness = dot(source.rgb * exp2(u_exposure), vec3(0.2126, 0.7152, 0.0722));
        float contribution = clamp(brightness - u_threshold, 0.0, 1.0);
        color = vec4(source.rgb * contribution, 1.0);
}
