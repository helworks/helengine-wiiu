#version 330

layout(location = 0) in vec4 aPosition;

layout(std140) uniform TransformBlock {
    mat4 uTransform;
};

void main() {
    gl_Position = uTransform * aPosition;
}
