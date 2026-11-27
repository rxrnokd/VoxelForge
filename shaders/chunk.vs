#version 330 core
layout(location = 0) in vec4 aVertex;
void main() { gl_Position = vec4(aVertex.xyz, 1.0); }
