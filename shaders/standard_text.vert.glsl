#version 450

layout(location = 0) in vec2 in_pos;

layout(push_constant) uniform constants
{
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    uint color;
    int window_width;
    int window_height;
} pc;

void main()
{
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
