#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D bitmap;

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

vec3 uint_color_to_vec3(uint color)
{
    uint r = color >>  0 & 0xff;
    uint g = color >>  8 & 0xff;
    uint b = color >> 16 & 0xff;
    return vec3(r/255.0f, g/255.0f, b/255.0f);
}

vec4 srgb_to_linear(vec4 srgb)
{
	vec3 b_less = step(vec3(0.04045),srgb.xyz);
	vec3 lin_out = mix(srgb.xyz/vec3(12.92), pow((srgb.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), b_less);
	return vec4(lin_out,srgb.w);
}

void main()
{
    // +--------------------+ <--- screen
    // |                    |
    // |    +----+ <-------------- text rectangle
    // |    |    |          |
    // |    |    |          |
    // |    +----+          |
    // |                    |
    // |                    |
    // +--------------------+

    int glyph_height = pc.y1 - pc.y0 + 1;
    int glyph_width  = pc.x1 - pc.x0 + 1;
    vec2 coords = in_uv*vec2(glyph_width, glyph_height);

    ivec2 texel_coords = ivec2(coords) + ivec2(pc.x0, pc.y0);
    // bool inside_bitmap = 0 <= texel_coords.x && texel_coords.x < 400 &&
    //                      0 <= texel_coords.y && texel_coords.y < 400;
    // if (!inside_bitmap) discard;
    float alpha = texelFetch(bitmap, texel_coords, 0).r;
    // out_color = srgb_to_linear(unpackUnorm4x8(pc.color));
    vec3 color_rgb = uint_color_to_vec3(pc.color);
    out_color = srgb_to_linear(vec4(color_rgb, alpha));
}
