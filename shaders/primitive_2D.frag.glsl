#version 450

#define SHAPE_2D_CIRCLE            0
#define SHAPE_2D_RECTANGLE         1
#define SHAPE_2D_RECTANGLE_ROUNDED 2

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform constants {
    int shape_2D;
    int x;
    int y;
    int width;
    int height;
    int radius;
    int window_width;
    int window_height;
    uint color;
} pc;

vec4 srgb_to_linear(vec4 srgb)
{
	vec3 b_less = step(vec3(0.04045),srgb.xyz);
	vec3 lin_out = mix(srgb.xyz/vec3(12.92), pow((srgb.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), b_less);
	return vec4(lin_out,srgb.w);;
}

void main()
{
    int radius = pc.radius;
    vec2 coords = in_uv*vec2(pc.width, pc.height);
    bool in_circles = length(coords - vec2(radius, radius)) < radius ||
                      length(coords - vec2(pc.width - radius, radius)) < radius ||
                      length(coords - vec2(radius, pc.height - radius)) < radius ||
                      length(coords - vec2(pc.width - radius, pc.height - radius)) < radius;
    bool in_cutout = (coords.x > radius && coords.x < pc.width - radius) ||
                     (coords.y > radius && coords.y < pc.height - radius);

    switch (pc.shape_2D) {
    case SHAPE_2D_CIRCLE:
        if (length(in_uv - vec2(0.5, 0.5)) > 0.5) discard;
    break;
    case SHAPE_2D_RECTANGLE_ROUNDED:
        if (!(in_circles || in_cutout)) discard;
    break;
    case SHAPE_2D_RECTANGLE:
    break;
    }
    out_color = srgb_to_linear(unpackUnorm4x8(pc.color));
    
}
