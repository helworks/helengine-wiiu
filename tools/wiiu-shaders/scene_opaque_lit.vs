#version 450

layout(std140, binding = 0) uniform TransformBlock {
    mat4 WorldMatrix;
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec3 aNormal;

layout(location = 0) out vec3 VertexNormal;

void main() {
    mat4 worldViewProjectionMatrix = ProjectionMatrix * ViewMatrix * WorldMatrix;
    VertexNormal = mat3(WorldMatrix) * aNormal;
    gl_Position = worldViewProjectionMatrix * aPosition;
}
