#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 VertexNormal;
layout(location = 1) out vec2 VertexTexCoord;

void main() {
    VertexNormal = aNormal;
    VertexTexCoord = aTexCoord;
    gl_Position = aPosition;
}
