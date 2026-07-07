#version 450

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

    bool inside_text_rect = pc.x <= gl_FragCoord.x && gl_FragCoord.x < pc.x + glyph_width &&
                            pc.y <= gl_FragCoord.y && gl_FragCoord.y < pc.y + glyph_height;
    if (inside_text_rect) {
        ivec2 texel_coords = ivec2(gl_FragCoord.x, gl_FragCoord.y) - ivec2(pc.x, pc.y) + ivec2(pc.x0, pc.y0);
        // bool inside_bitmap = 0 <= texel_coords.x && texel_coords.x < 400 &&
        //                      0 <= texel_coords.y && texel_coords.y < 400;
        // if (!inside_bitmap) discard;
        float alpha = texelFetch(bitmap, texel_coords, 0).r;
        vec3 color_rgb = uint_color_to_vec3(pc.color);
        out_color = vec4(color_rgb, alpha);
    } else {
        discard;
    }
}
