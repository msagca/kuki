#version 460 core
const uint TONE_MAPPER_NONE = 0u;
const uint TONE_MAPPER_REINHARD = 1u;
const uint TONE_MAPPER_ACES = 2u;
const uint TONE_MAPPER_AGX = 3u;
in vec2 v_texCoords;
out vec4 color;
uniform float u_exposure;
uniform float u_gamma;
uniform uint u_toneMapper;
uniform sampler2D u_image;
vec3 ACESNarkowicz(vec3);
vec3 AgX(vec3);
vec3 AgXContrast(vec3);
vec3 Encode(vec3);
void main() {
        // Exposure is a plain scale in stops, so a step of one doubles the light handed to the curve.
        // It belongs here rather than in the lighting because it describes the camera rather than the
        // scene, and applying it once at the end leaves every earlier pass working in the radiance the
        // lights actually emit.
        vec3 radiance = texture(u_image, v_texCoords).rgb * exp2(u_exposure);
        vec3 display;
        if (u_toneMapper == TONE_MAPPER_REINHARD)
                display = Encode(radiance / (radiance + vec3(1.0)));
        else if (u_toneMapper == TONE_MAPPER_ACES)
                display = Encode(ACESNarkowicz(radiance));
        else if (u_toneMapper == TONE_MAPPER_AGX)
                // Deliberately not through `Encode`: AgX ends in display space already. See `AgX`.
                display = AgX(radiance);
        else
                display = Encode(radiance);
        color = vec4(display, 1.0);
}
// Radiance to display, which is the clip and the transfer curve every operator but AgX still needs.
vec3 Encode(vec3 radiance) {
        return pow(clamp(radiance, 0.0, 1.0), vec3(1.0 / u_gamma));
}
vec3 ACESNarkowicz(vec3 x) {
        return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}
// Sixth-order fit to AgX's contrast sigmoid, to a mean squared error around 3.7e-06, which is far
// below a display's smallest step and so is exact as far as the image is concerned.
vec3 AgXContrast(vec3 x) {
        vec3 x2 = x * x;
        vec3 x4 = x2 * x2;
        return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
// AgX works in log space rather than on the radiance directly: the scene is rotated into a slightly
// desaturated working set of primaries, mapped onto a fixed window of log2 stops, run through the
// sigmoid, and rotated back. Compressing in log means a colour approaches white by losing saturation
// rather than by having its largest channel clipped, and that is what holds the hue steady.
//
// No look transform is layered on top, so this is the neutral AgX rather than the punchier curve
// Blender shows by default. Middle grey leaves it at about 0.50.
//
// The result leaves display encoded, because the sigmoid maps onto the display's range directly.
// Reference implementations then raise it to 2.2 to undo that, since they hand the result to an sRGB
// render target that applies the curve again. This pipeline writes to a linear target and encodes by
// hand, so both of those steps are dropped rather than performed and immediately undone.
vec3 AgX(vec3 radiance) {
        const mat3 toAgX = mat3(
                0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                0.0784335999999992, 0.878468636469772, 0.0784336,
                0.0792237451477643, 0.0791661274605434, 0.879142973793104);
        const mat3 fromAgX = mat3(
                1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
        const float minEV = -12.47393;
        const float maxEV = 4.026069;
        // The rotation carries negative off-diagonal terms, so a saturated primary can leave it just
        // below zero. `log2` of a negative is a NaN, and a NaN survives the clamp that follows and
        // spreads back through the second rotation, so it is cut off here rather than tested for
        // afterwards.
        vec3 working = max(toAgX * radiance, vec3(0.0));
        working = (clamp(log2(working), minEV, maxEV) - minEV) / (maxEV - minEV);
        return clamp(fromAgX * AgXContrast(working), 0.0, 1.0);
}
