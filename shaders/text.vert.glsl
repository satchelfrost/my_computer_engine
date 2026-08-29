#version 450

layout(location = 0) out vec2 out_uv;

/*
 *   1----------3
 *   | .        |
 *   |   .      |
 *   |     .    |
 *   |       .  |
 *   0----------2
 */

layout(push_constant) uniform constants {
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

const vec2 uvs[6] = vec2[](
    vec2(0,1),
    vec2(0,0),
    vec2(1,1),
    vec2(1,1),
    vec2(0,0),
    vec2(1,0)
);

void main()
{
    int height = pc.y1 - pc.y0 + 1;
    int width  = pc.x1 - pc.x0 + 1;
    vec2 ndc0 = vec2( pc.x       /float(pc.window_width), (pc.y+height)/float(pc.window_height))*vec2(2.0) - vec2(1.0);
    vec2 ndc1 = vec2( pc.x       /float(pc.window_width),  pc.y        /float(pc.window_height))*vec2(2.0) - vec2(1.0);
    vec2 ndc2 = vec2((pc.x+width)/float(pc.window_width), (pc.y+height)/float(pc.window_height))*vec2(2.0) - vec2(1.0);
    vec2 ndc3 = vec2((pc.x+width)/float(pc.window_width),  pc.y        /float(pc.window_height))*vec2(2.0) - vec2(1.0);

    // hmm... no bounds checking

    switch (gl_VertexIndex) {
    case 0: gl_Position = vec4(ndc0, 0.0, 1.0); out_uv = uvs[0]; break;
    case 1: gl_Position = vec4(ndc1, 0.0, 1.0); out_uv = uvs[1]; break;
    case 2: gl_Position = vec4(ndc2, 0.0, 1.0); out_uv = uvs[2]; break;
    case 3: gl_Position = vec4(ndc2, 0.0, 1.0); out_uv = uvs[3]; break;
    case 4: gl_Position = vec4(ndc1, 0.0, 1.0); out_uv = uvs[4]; break;
    case 5: gl_Position = vec4(ndc3, 0.0, 1.0); out_uv = uvs[5]; break;
    }
}
