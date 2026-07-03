#version 450

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec4 in_tanget;
layout(location = 4) in vec3 in_position;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform constants {
    mat4 model;
    uint color;
    uint attributes;
    uint flags;
} pc;

#define ATTRIBUTE_NORMAL    (1<<0)
#define ATTRIBUTE_TEX_COORD (1<<1)
#define ATTRIBUTE_TANGET    (1<<2)
#define ATTRIBUTE_COLOR     (1<<3)

#define AMBIENT 0.1f

void main()
{
    if ((pc.attributes&ATTRIBUTE_NORMAL) > 0) {
        vec4 world_pos = pc.model*vec4(in_position, 1.0);
        vec3 light_pos = vec3(5.0, 10.0, 0.0);
        vec3 n = normalize(in_normal);
        n = normalize(transpose(inverse(mat3(pc.model)))*n);
        vec3 to_light = normalize(light_pos - world_pos.xyz);
        float diffuse = max(dot(n, to_light), AMBIENT);
        vec3 rgb = in_color.rgb*diffuse;
        out_color = vec4(rgb, 1.0);
    } else {
        out_color = in_color;
    }

}
