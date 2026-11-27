#version 330 core

in vec2 vNdc;

uniform mat4 uInvViewProj;   // inverse camera-relative view-projection
uniform vec3 uSunDir;
uniform vec3 uZenith;
uniform vec3 uHorizon;
uniform float uDaylight;
uniform float uStars;

out vec4 fragColor;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

// Square, pixel-art style celestial body centred on `axis`.
float squareDisc(vec3 dir, vec3 axis, float halfSize) {
    if (dot(dir, axis) <= 0.0) return 0.0;
    vec3 up = abs(axis.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, axis));
    vec3 top = cross(axis, right);
    vec2 p = vec2(dot(dir, right), dot(dir, top)) / dot(dir, axis);
    return step(max(abs(p.x), abs(p.y)), halfSize);
}

void main() {
    vec4 far = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w);

    float h = dir.y;
    vec3 color = mix(uHorizon, uZenith, pow(clamp(h, 0.0, 1.0), 0.5));
    if (h < 0.0) {
        color = mix(uHorizon, uHorizon * 0.55, clamp(-h * 2.5, 0.0, 1.0));
    }

    // Glow around the sun, warmer near the horizon.
    float sunDot = max(dot(dir, uSunDir), 0.0);
    vec3 glowColor = mix(vec3(1.0, 0.55, 0.25), vec3(1.0, 0.95, 0.8), clamp(uSunDir.y * 3.0, 0.0, 1.0));
    color += glowColor * pow(sunDot, 48.0) * 0.45 * smoothstep(-0.2, 0.1, uSunDir.y);

    color = mix(color, vec3(1.0, 0.97, 0.85), squareDisc(dir, uSunDir, 0.055));
    float moon = squareDisc(dir, -uSunDir, 0.04);
    color = mix(color, vec3(0.86, 0.89, 0.98), moon * (1.0 - uDaylight * 0.85));

    if (uStars > 0.0 && h > 0.0) {
        vec3 cell = floor(dir * 280.0);
        float n = hash13(cell);
        float star = smoothstep(0.9965, 1.0, n) * uStars * clamp(h * 5.0, 0.0, 1.0);
        color += vec3(star);
    }
    fragColor = vec4(color, 1.0);
}
