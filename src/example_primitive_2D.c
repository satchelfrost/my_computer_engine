#include "my_computer.h"

int main()
{
    init_window(500, 500, "primitive 2D");
    while (!window_should_close()) {
        begin_drawing(BLUE);
            // draw_circle(250, 250, 50, GREEN);
            draw_rectangle(250, 250, 200, 100, PURPLE);
        end_drawing();
    }
    close_window();
    return 0;
}
