#version 440

layout(location = 0) in vec4 position;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
} ubuf;


void main()
{
    gl_Position = ubuf.mvp * position;
}
