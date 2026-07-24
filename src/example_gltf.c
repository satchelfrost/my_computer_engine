#include "my_computer.h"

#define GLTF_MODEL "assets/psx/scene.gltf"
// #define GLTF_MODEL "assets/robot.glb"

int main()
{
    printf("loading model %s...\n", GLTF_MODEL);
    Model model = load_model_from_gltf_into_memory(GLTF_MODEL);
    printf("loaded model %s\n", GLTF_MODEL);

    init_window(500, 500, "gltf");

    load_model_gpu(&model);
    Camera camera = {
        .position = {0.0f, 2.0f, 5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    while (!window_should_close()) {
        update_camera_free(&camera);
        begin_drawing(BLUE);
            begin_mode_3D(camera);
                rotate_y(get_time());
                scale(0.01, 0.01, 0.01);
                draw_model(model);
            end_mode_3D();
        end_drawing();
    }

    destroy_model(model);

    close_window();

    return 0;
}
