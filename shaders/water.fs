#version 330 core

in vec3 vLocalPos;
in vec3 vViewPos;
in float vAo;
flat in int vFace;
flat in int vTile;

uniform sampler2D uAtlas;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbient;
uniform vec3 uFogColor;
uniform vec2 uFogRange;
uniform float uTime;
uniform float uAlpha;
uniform vec3 uCameraPos;

out vec4 fragColor;

const vec3 NORMALS[6] = vec3[6](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0),
                                vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0));
const int TILE_PIXELS = 16;
const int ATLAS_COLUMNS = 16;

vec2 faceTexel(vec3 p, int face) {
    vec2 uv;
    if (face < 2)      uv = vec2(p.z, 256.0 - p.y);
    else if (face < 4) uv = vec2(p.x, p.z);
    else               uv = vec2(p.x, 256.0 - p.y);
    return uv * float(TILE_PIXELS);
}

void main() {
    vec3 normal = NORMALS[vFace];

    // Scroll the water texture in whole texels for a pixel-art ripple.
    vec2 texel = faceTexel(vLocalPos, vFace) + floor(vec2(uTime * 3.0, uTime * 1.7));
    ivec2 local = ivec2(floor(texel)) & (TILE_PIXELS - 1);
    ivec2 origin = ivec2(vTile % ATLAS_COLUMNS, vTile / ATLAS_COLUMNS) * TILE_PIXELS;
    vec3 albedo = texelFetch(uAtlas, origin + local, 0).rgb;

    // Gentle analytic ripples perturb the normal for the specular highlight.
    vec2 w = vViewPos.xz + uCameraPos.xz; // world space so ripples continue across chunks
    vec3 rippled = normalize(normal + vec3(sin(w.x * 1.7 + uTime * 1.3), 0.0, cos(w.y * 1.9 + uTime * 1.1)) * 0.06);

    vec3 viewDir = normalize(-vViewPos);
    float direct = max(dot(normal, uSunDir), 0.0);
    vec3 halfway = normalize(uSunDir + viewDir);
    float spec = pow(max(dot(rippled, halfway), 0.0), 96.0);

    vec3 color = albedo * (uAmbient + uSunColor * direct) + uSunColor * spec * 0.8;

    // Fresnel: water becomes more opaque at grazing angles.
    float facing = abs(dot(normal, viewDir));
    float alpha = clamp(mix(uAlpha + 0.2, uAlpha, facing) + spec * 0.5, 0.0, 0.95);

    float dist = length(vViewPos.xz);
    float fog = smoothstep(uFogRange.x, uFogRange.y, dist);
    fragColor = vec4(mix(color, uFogColor, fog), mix(alpha, 1.0, fog));
}
