#version 450

layout(location = 0) in vec4 aPosition;

layout(std140, binding = 2) uniform ShadowMatrixBlock {
    mat4 LightViewProjection;
};

void main() {
    gl_Position = LightViewProjection * aPosition;
}
