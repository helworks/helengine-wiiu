#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(std140, binding = 2) uniform ShadowMatrixBlock {
    mat4 LightViewProjection;
};

layout(location = 0) out vec3 VertexNormal;
layout(location = 1) out vec2 VertexTexCoord;
layout(location = 2) out vec4 ShadowPosition;

void main() {
    VertexNormal = aNormal;
    VertexTexCoord = aTexCoord;
    ShadowPosition = LightViewProjection * aPosition;
    gl_Position = aPosition;
}
