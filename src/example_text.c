#include "my_computer.h"

int main()
{
    init_window(800, 600, "my_computer_engine - example text");

    const char *text = "Hello World!";
    size_t text_len = strlen(text);
    int text_width = measure_text(text, text_len);

    while (!window_should_close()) {
        begin_drawing(WHITE);
            draw_text_at_base(text, strlen(text), 800/2 - text_width/2, 600/2, BLACK);
        end_drawing();
    }

    close_window();

    return 0;
}
