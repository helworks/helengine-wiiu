#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec3 aNormal;

layout(location = 0) out vec3 VertexNormal;

void main() {
    VertexNormal = aNormal;
    gl_Position = aPosition;
}
