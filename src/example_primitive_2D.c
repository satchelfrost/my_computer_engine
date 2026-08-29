#include "my_computer.h"

int main()
{
    init_window(500, 500, "primitive 2D");
    float radius = 15;
    String_Builder sb = {0};
    while (!window_should_close()) {
        float dt = get_frame_time();
        sb.count = 0;
        if (is_key_down(KEY_DOWN)) radius -= dt*30;
        if (is_key_down(KEY_UP))   radius += dt*30;
        radius = (radius < 0) ? 0 : radius;
        radius = (radius > 50) ? 50 : radius;
        sb_appendf(&sb, "Radius %.2f\n", radius);
        begin_drawing(BLUE);
            draw_rectangle_rounded(250-100, 250-50, 200, 100, radius, PURPLE);
            draw_text_at_base(sb.items, sb.count, 5, 38, BLACK);
        end_drawing();
    }
    close_window();
    return 0;
}
