#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) out vec4 VertexColor;

void main() {
    TexCoord = aTexCoord;
    VertexColor = aColor;
    gl_Position = vec4(aPos.x, aPos.y, 0.0f, 1.0f);
}
