#include "my_computer.h"

#define FONT_SIZE 42

int main()
{
    init_window(1600, 900, "example text");

    Font font = load_font("assets/RobotoMono-Medium.ttf", FONT_SIZE);
    String_Builder sb = {0};

    while (!window_should_close()) {
        begin_drawing(BLUE);
            sb.count = 0;
            sb_appendf(&sb, "FPS:%d", get_avg_fps());
            draw_text_at_base(font, sb.items, sb.count, 20, FONT_SIZE, BLACK);
        end_drawing();
    }

    close_window();

    return 0;
}
