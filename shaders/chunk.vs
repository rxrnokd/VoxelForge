#version 330 core

// Packed chunk vertex: xyz = fixed point position (1/16 block) relative to the chunk
// origin, w = bits 0-2 face, bits 3-4 ambient occlusion, bits 5-12 atlas tile,
// bits 13-15 sky exposure (0..4).
layout(location = 0) in vec4 aVertex;

uniform mat4 uViewProj;     // camera-relative view-projection (no translation)
uniform vec3 uChunkOffset;  // chunk origin minus camera position

out vec3 vLocalPos;         // chunk-local position, drives texture coordinates
out vec3 vViewPos;          // camera-relative position, drives fog and specular
out float vAo;              // 0 = fully occluded corner, 1 = open
out float vSky;             // 0 = no sky visible (caves, overhangs), 1 = open sky
flat out int vFace;
flat out int vTile;

void main() {
    int data = int(aVertex.w + 0.5);
    vFace = data & 7;
    vAo = float((data >> 3) & 3) / 3.0;
    vTile = (data >> 5) & 255;
    vSky = float((data >> 13) & 7) / 4.0;

    vLocalPos = aVertex.xyz / 16.0;
    vViewPos = uChunkOffset + vLocalPos;
    gl_Position = uViewProj * vec4(vViewPos, 1.0);
}
