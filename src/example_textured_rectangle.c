#include "my_computer_engine.h"

#define SCREEN_WIDTH 500
#define SCREEN_HEIGHT 500
#define SPEED 100

int main()
{
    init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "textured rectangle");

    Texture_2D texture = load_texture("images/walkman.png");
    int rect_width = texture.width*0.5;
    int rect_height = texture.height*0.5;

    Vector2 pos = {0,0};
    Vector2 vel = {1*SPEED,2*SPEED};

    set_target_fps(60);
    while (!window_should_close()) {
        float dt = get_frame_time();

        float nx = pos.x + vel.x*dt;
        float ny = pos.y + vel.y*dt;
        if (0 <= nx && nx < SCREEN_WIDTH - rect_width) pos.x = nx;
        else                                           vel.x *= -1.0;
        if (0 <= ny && ny < SCREEN_HEIGHT - rect_height) pos.y = ny;
        else                                             vel.y *= -1.0;

        begin_drawing(BLACK);
            draw_rectangle_rounded_texture(texture, pos.x, pos.y, rect_width, rect_height, 12);
            draw_fps();
        end_drawing();
    }

    unload_texture(texture);
    close_window();
    return 0;
}
