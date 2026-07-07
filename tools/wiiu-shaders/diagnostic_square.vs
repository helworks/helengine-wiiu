#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 VertexColor;

void main() {
    VertexColor = aColor;
    gl_Position = aPosition;
}
