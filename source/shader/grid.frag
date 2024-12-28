#version 330
in vec3 worldPos;
layout(location = 0) out vec4 color;
uniform vec3 cameraPos;
uniform float gridSize = 100.0;
uniform float minPixelsBetweenCells = 2.0;
uniform float cellSize = 0.025;
uniform vec4 colorThin = vec4(0.5, 0.5, 0.5, 1.0);
uniform vec4 colorThick = vec4(0.0, 0.0, 0.0, 1.0);
float log10(float x) {
    float f = log(x) / log(10.0);
    return f;
}
float satf(float x) {
    float f = clamp(x, 0.0, 1.0);
    return f;
}
vec2 satv(vec2 x) {
    vec2 v = clamp(x, vec2(0.0), vec2(1.0));
    return v;
}
float max2(vec2 v) {
    float f = max(v.x, v.y);
    return f;
}
void main() {
    vec2 dvx = vec2(dFdx(worldPos.x), dFdy(worldPos.x));
    vec2 dvy = vec2(dFdx(worldPos.z), dFdy(worldPos.z));
    float lx = length(dvx);
    float ly = length(dvy);
    vec2 dudv = vec2(lx, ly);
    float l = length(dudv);
    float lod = max(0.0, log10(l * minPixelsBetweenCells / cellSize) + 1.0);
    float cellSizeLOD0 = cellSize * pow(10.0, floor(lod));
    float cellSizeLOD1 = cellSizeLOD0 * 10.0;
    float cellSizeLOD2 = cellSizeLOD1 * 10.0;
    dudv *= 4.0;
    vec2 modDivDuDv = mod(worldPos.xz, cellSizeLOD0) / dudv;
    float lod0a = max2(vec2(1.0) - abs(satv(modDivDuDv) * 2.0 - vec2(1.0)));
    modDivDuDv = mod(worldPos.xz, cellSizeLOD1) / dudv;
    float lod1a = max2(vec2(1.0) - abs(satv(modDivDuDv) * 2.0 - vec2(1.0)));
    modDivDuDv = mod(worldPos.xz, cellSizeLOD2) / dudv;
    float lod2a = max2(vec2(1.0) - abs(satv(modDivDuDv) * 2.0 - vec2(1.0)));
    float lodFade = fract(lod);
    vec4 colorTemp;
    if (lod2a > 0.0) {
        colorTemp = colorThick;
        colorTemp.a *= lod2a;
    } else if (lod1a > 0.0) {
        colorTemp = mix(colorThick, colorThin, lodFade);
        colorTemp.a *= lod1a;
    } else {
        colorTemp = colorThin;
        colorTemp.a *= (lod0a * (1.0 - lodFade));
    }
    float opacityFalloff = (1.0 - satf(length(worldPos.xz - cameraPos.xz) / gridSize));
    colorTemp.a *= opacityFalloff;
    color = colorTemp;
}
