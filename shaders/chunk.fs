#version 330 core

in vec3 vLocalPos;
in vec3 vViewPos;
in float vAo;
in float vSky;
flat in int vFace;
flat in int vTile;

uniform sampler2D uAtlas;
uniform vec3 uSunDir;        // direction towards the sun (or moon at night)
uniform vec3 uSunColor;      // direct light colour * intensity
uniform vec3 uAmbient;       // sky ambient colour * intensity
uniform vec3 uFogColor;
uniform vec2 uFogRange;      // start, end distance

out vec4 fragColor;

const vec3 NORMALS[6] = vec3[6](vec3(-1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0),
                                vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0));
// Fixed per-face ambient shading keeps block shapes readable even without sunlight.
const float FACE_SHADE[6] = float[6](0.84, 0.84, 0.58, 1.0, 0.72, 0.72);

const int TILE_PIXELS = 16;
const int ATLAS_COLUMNS = 16;
const float MAX_TILE_LOD = 4.0;

// Continuous texel coordinates (16 per block) on the face plane. Greedy quads span many
// blocks; the integer part wraps per block, so textures tile without per-vertex UVs.
vec2 faceTexel(vec3 p, int face) {
    vec2 uv;
    if (face == 0)      uv = vec2(p.z, 256.0 - p.y);
    else if (face == 1) uv = vec2(16.0 - p.z, 256.0 - p.y);
    else if (face == 4) uv = vec2(16.0 - p.x, 256.0 - p.y);
    else if (face == 5) uv = vec2(p.x, 256.0 - p.y);
    else                uv = vec2(p.x, p.z);
    return uv * float(TILE_PIXELS);
}

vec4 sampleAtlas(vec2 texel, int tile) {
    // Manual mip selection: atlas mips are built per tile, texelFetch avoids bleeding.
    vec2 dx = dFdx(texel);
    vec2 dy = dFdy(texel);
    float rho = max(dot(dx, dx), dot(dy, dy));
    int level = int(clamp(0.5 * log2(max(rho, 1e-8)) + 0.25, 0.0, MAX_TILE_LOD));
    int size = TILE_PIXELS >> level;
    ivec2 local = (ivec2(floor(texel)) & (TILE_PIXELS - 1)) >> level;
    ivec2 origin = ivec2(tile % ATLAS_COLUMNS, tile / ATLAS_COLUMNS) * size;
    return texelFetch(uAtlas, origin + local, level);
}

void main() {
    vec4 albedo = sampleAtlas(faceTexel(vLocalPos, vFace), vTile);
    if (albedo.a < 0.5) {
        discard; // cut-out pixels (glass)
    }

    vec3 normal = NORMALS[vFace];
    float direct = max(dot(normal, uSunDir), 0.0);
    float ao = mix(0.38, 1.0, vAo * vAo * (3.0 - 2.0 * vAo));
    // Without sky exposure only a dim fraction of the ambient light remains (caves,
    // overhangs, the ground under tree canopies); direct light needs an open sky.
    vec3 caveAmbient = uAmbient * 0.3 + vec3(0.085, 0.085, 0.10);
    vec3 ambient = mix(caveAmbient, uAmbient, vSky);
    vec3 light = (ambient * FACE_SHADE[vFace] + uSunColor * direct * vSky * vSky) * ao;
    vec3 color = albedo.rgb * light;

    float dist = length(vViewPos.xz);
    float fog = smoothstep(uFogRange.x, uFogRange.y, dist);
    fragColor = vec4(mix(color, uFogColor, fog), 1.0);
}
