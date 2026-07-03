#version 450

#define FLAG_STEREO         (1<<0)
#define ATTRIBUTE_NORMAL    (1<<0)
#define ATTRIBUTE_TEX_COORD (1<<1)
#define ATTRIBUTE_TANGET    (1<<2)
#define ATTRIBUTE_COLOR     (1<<3)

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 color;
layout(location = 3) out vec4 tanget;
layout(location = 4) out vec3 out_position;

#define EYE_LEFT  0
#define EYE_RIGHT 1
#define EYE_COUNT 2

layout(binding = 0) uniform UBO {
    mat4 proj[EYE_COUNT];
    mat4 view[EYE_COUNT];
} ubo;

layout(std430, binding = 1) buffer normal_data {
   float normals[ ];
};

layout(std430, binding = 2) buffer tex_coord_data {
   float tex_coords[ ];
};

layout(std430, binding = 3) buffer color_data {
   uint colors[ ];
};

layout(std430, binding = 4) buffer tanget_data {
   float tangets[ ];
};

layout(push_constant) uniform constants {
    mat4 model;
    uint color;
    uint attributes;
    uint flags;
} pc;

void main()
{
    gl_Position = ubo.proj[0]*ubo.view[0]*pc.model*vec4(position, 1.0);

    if ((pc.attributes&ATTRIBUTE_NORMAL) > 0) {
        normal = vec3(normals[gl_VertexIndex*3+0],normals[gl_VertexIndex*3+1],normals[gl_VertexIndex*3+2]);
    } else {
        normal = vec3(0.0);
    }

    uv = vec2(0.0);

    if ((pc.attributes&ATTRIBUTE_COLOR) > 0) {
        color = unpackUnorm4x8(colors[gl_VertexIndex]);
    } else {
        color = unpackUnorm4x8(pc.color);
    }

    out_position = position;

    tanget = vec4(0.0);
}
