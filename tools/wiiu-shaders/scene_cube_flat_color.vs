#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 0) out vec4 VertexColor;

void main() {
    gl_Position = aPosition;
    VertexColor = vec4(0.92, 0.78, 0.32, 1.0);
}
