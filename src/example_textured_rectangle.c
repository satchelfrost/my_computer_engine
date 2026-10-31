#include "my_computer.h"

int main()
{
    init_window(500, 500, "textured rectangle");

    while (!window_should_close()) {
        begin_drawing(GRAY);
//        draw_rectangle
        end_drawing();
    }

    close_window();
    return 0;
}
