#version 450

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec4 in_tanget;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = in_color;
}
