#include "my_computer.h"

#define GLTF_MODEL "assets/robot.glb"
// #define GLTF_MODEL "assets/sponza/Sponza.gltf"

int main()
{
#if 1
    Model_Animations anims = load_model_animations_gltf(GLTF_MODEL);
    printf("loaded %zu animations from %s\n", anims.count, GLTF_MODEL);
    return 0;

#else
    init_window(500, 500, "gltf animation");
    Model robot = load_model_gltf(GLTF_MODEL);

    Camera camera = {
        .position = {0.0f, 7.0f, 10.0f},
        .target   = {0.0f, 3.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    while (!window_should_close()) {
        begin_drawing(BLUE); {
            begin_mode_3D(camera); {
                rotate_y(get_time());
                draw_model(robot);
            } end_mode_3D();

            draw_fps();
        } end_drawing();
    }

    destroy_model(robot);
    close_window();
    return 0;
#endif
}
