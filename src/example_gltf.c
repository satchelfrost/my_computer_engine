#include "my_computer.h"

int main()
{
    init_window(500, 500, "gltf");

    Model psx = load_model_from_gltf("assets/psx/scene.gltf");

    Camera camera = {
        .position = {0.0f, 2.0f, 5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    /* this particular asset has separate meshes, which are actually models */
    int mesh_idx = 0;

    while (!window_should_close()) {
        update_camera_free(&camera);

        if (is_key_pressed(KEY_SPACE)) mesh_idx = (mesh_idx + 1)%psx.meshes.count;

        begin_drawing(DARKGRAY); {
            begin_mode_3D(camera); {
                rotate_y(get_time());
                scale(0.01, 0.01, 0.01);
                draw_mesh(psx.meshes.items[mesh_idx]);
            } end_mode_3D();
            draw_fps();
        } end_drawing();
    }

    destroy_model(psx);

    close_window();

    return 0;
}
