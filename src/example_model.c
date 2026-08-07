#include "my_computer.h"

#define FONT_SIZE 42

uint32_t rand_color()
{
    Color c = {
        .r = rand()/(float)RAND_MAX*255,
        .g = rand()/(float)RAND_MAX*255,
        .b = rand()/(float)RAND_MAX*255,
        .a = 255,
    };
    return color_to_uint32_t(c);
}

int main()
{
    init_window(800, 450, "cube");
    Model bunny  = load_model_from_obj("assets/bunny.obj");
    bunny.meshes.items[0].material.color = GOLD;
    Shape_3D shape = SHAPE_3D_CUBE;

    /* show individual triangles of bunny model */
    if (0) {
        size_t tri_count = bunny.meshes.items[0].cpu.indices.count/3;
        for (size_t i = 0; i < tri_count; i++) {
            uint32_t c = rand_color();
            da_append(&bunny.meshes.items[0].cpu.colors, c);
            da_append(&bunny.meshes.items[0].cpu.colors, c);
            da_append(&bunny.meshes.items[0].cpu.colors, c);
        }
    }

    Camera camera = {
        .position = {0.0f, 2.0f, 5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    while (!window_should_close()) {
        if (is_key_down(KEY_F)) log_fps();
        if (is_key_pressed(KEY_SPACE)) {
            /* 1 extra shape for the bunny model */
            shape = (shape + 1)%(SHAPE_3D_COUNT + 1);
        }

        update_camera_free(&camera);

        begin_drawing(BLACK);
            begin_mode_3D(camera);
                rotate_y(get_time());
                if (shape == SHAPE_3D_COUNT) draw_model(bunny);
                else                         draw_shape_3D(shape);
            end_mode_3D();

            draw_fps();
        end_drawing();
    }

    destroy_model(bunny);
    close_window();

    return 0;
}
