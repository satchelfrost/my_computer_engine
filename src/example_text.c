#include "my_computer.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

int main()
{
    init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "example text");

    const char *text = "Hello Wolrd!";
    size_t text_len = strlen(text);
    int text_width = measure_text(text, text_len);
    Color bg = {30, 30, 30, 255};

    while (!window_should_close()) {
        begin_drawing(bg);
            draw_text_at_base(text, text_len, WINDOW_WIDTH/2 - text_width/2, WINDOW_HEIGHT/2, PURPLE);
        end_drawing();
    }

    close_window();

    return 0;
}
