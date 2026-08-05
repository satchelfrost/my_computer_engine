#version 450

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec4 in_tanget;
layout(location = 4) in vec3 in_position;
layout(location = 5) in vec4 in_joint;
layout(location = 6) in vec4 in_weight;

layout(location = 0) out vec4 out_color;

layout (binding = 7) uniform sampler2D sampler_color;
layout (binding = 8) uniform sampler2D sampler_normal;
layout (binding = 9) uniform sampler2D sampler_metallic_roughness;


layout(push_constant) uniform constants {
    mat4 model;
    uint color;
    uint attribute_mask;
    uint material_mask;
} pc;

#define ATTRIBUTE_NORMAL (1<<0)
#define ATTRIBUTE_UV     (1<<1)
#define ATTRIBUTE_TANGET (1<<2)
#define ATTRIBUTE_COLOR  (1<<3)
#define ATTRIBUTE_JOINT  (1<<4)
#define ATTRIBUTE_WEIGHT (1<<4)

#define MATERIAL_NO_TEXTURES        (1<<0)
#define MATERIAL_ALBEDO             (1<<1)
#define MATERIAL_METALLIC_ROUGHNESS (1<<2)
#define MATERIAL_NORMAL             (1<<3)

#define AMBIENT 0.3f

vec4 srgb_to_linear(vec4 srgb)
{
	vec3 b_less = step(vec3(0.04045),srgb.xyz);
	vec3 lin_out = mix(srgb.xyz/vec3(12.92), pow((srgb.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), b_less);
	return vec4(lin_out,srgb.w);;
}

void main()
{
    vec4 color = in_color;
    if ((pc.material_mask&MATERIAL_ALBEDO) > 0) {
        color = texture(sampler_color, in_uv);
    }

    if ((pc.attribute_mask&ATTRIBUTE_NORMAL) > 0) {
        vec4 world_pos = pc.model*vec4(in_position, 1.0);
        vec3 light_pos = vec3(2.0, 2.0, 5.0);
        vec3 n = normalize(in_normal);
        n = normalize(transpose(inverse(mat3(pc.model)))*n);
        vec3 to_light = normalize(light_pos - world_pos.xyz);
        float diffuse = max(dot(n, to_light), AMBIENT);
        vec3 rgb = color.rgb*diffuse;
        // vec3 rgb = color.rgb;
        out_color = vec4(rgb, 1.0);
    } else {
        out_color = color;
    }

    out_color = srgb_to_linear(out_color);

}
