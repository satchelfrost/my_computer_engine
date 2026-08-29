#version 450

#define FLAG_STEREO         (1<<0)

#define ATTRIBUTE_NORMAL (1<<0)
#define ATTRIBUTE_UV     (1<<1)
#define ATTRIBUTE_TANGET (1<<2)
#define ATTRIBUTE_COLOR  (1<<3)
#define ATTRIBUTE_JOINT  (1<<4)
#define ATTRIBUTE_WEIGHT (1<<4)

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec4 out_tanget;
layout(location = 4) out vec3 out_position;
layout(location = 5) out vec4 out_joint;
layout(location = 6) out vec4 out_weight;

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

layout(std430, binding = 2) buffer uv_data {
    float uvs[ ];
};

layout(std430, binding = 3) buffer color_data {
    uint colors[ ];
};

layout(std430, binding = 4) buffer tanget_data {
    float tangets[ ];
};

layout(std430, binding = 5) buffer joint_data {
    uint joints[ ];
};

layout(std430, binding = 6) buffer weight_data {
    float weights[ ];
};

layout(push_constant) uniform constants {
    mat4 model;
    uint color;
    uint attribute_mask;
    uint material_mask;
} pc;

void main()
{
    gl_Position = ubo.proj[0]*ubo.view[0]*pc.model*vec4(position, 1.0);

    out_position = position;

    // TODO: I should be doing the transpose of the inverse here to save a little performance
    if ((pc.attribute_mask&ATTRIBUTE_NORMAL) > 0)
        out_normal = vec3(normals[gl_VertexIndex*3+0],normals[gl_VertexIndex*3+1],normals[gl_VertexIndex*3+2]);
    else
        out_normal = vec3(0.0);

    if ((pc.attribute_mask&ATTRIBUTE_UV) > 0)
        out_uv = vec2(uvs[gl_VertexIndex*2+0],uvs[gl_VertexIndex*2+1]);
    else
        out_uv = vec2(0.0);

    if ((pc.attribute_mask&ATTRIBUTE_COLOR) > 0)
        out_color = unpackUnorm4x8(colors[gl_VertexIndex]);
    else
        out_color = unpackUnorm4x8(pc.color);

    if ((pc.attribute_mask&ATTRIBUTE_TANGET) > 0)
        out_tanget = vec4(tangets[gl_VertexIndex*4+0],tangets[gl_VertexIndex*4+1],tangets[gl_VertexIndex*4+2], tangets[gl_VertexIndex*4+3]);
    else
        out_tanget = vec4(0.0);

    if ((pc.attribute_mask&ATTRIBUTE_JOINT) > 0)
        out_joint = vec4(joints[gl_VertexIndex*4+0],joints[gl_VertexIndex*4+1],joints[gl_VertexIndex*4+2], joints[gl_VertexIndex*4+3]);
    else
        out_joint = vec4(0.0);

    if ((pc.attribute_mask&ATTRIBUTE_WEIGHT) > 0)
        out_weight = vec4(weights[gl_VertexIndex*4+0],weights[gl_VertexIndex*4+1],weights[gl_VertexIndex*4+2], weights[gl_VertexIndex*4+3]);
    else
        out_weight = vec4(0.0);
}
