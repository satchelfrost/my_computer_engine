#include "my_computer_engine.h"

#define GLTF_MODEL "assets/robot.glb"
// #define GLTF_MODEL "assets/sponza/Sponza.gltf"

int main()
{
    set_target_fps(60);
    init_window(500, 500, "gltf animation");
    Model model = load_model_gltf(GLTF_MODEL);
    Model_Animations anims = load_model_animations_gltf(GLTF_MODEL);

    if (anims.count) {
        printf("loaded %zu animations from %s\n", anims.count, GLTF_MODEL);
    } else {
        printf("model %s does not contain any animations\n", GLTF_MODEL);
        return 1;
    }

    Camera camera = {
        .position = {0.0f, 7.0f, 10.0f},
        .target   = {0.0f, 3.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    size_t anim_curr_frame = 0;
    int anim_index = 0;
    Model_Animation animation = anims.items[0];

    while (!window_should_close()) {
        if (is_key_pressed(KEY_RIGHT)) {
            anim_index = (anim_index + 1)%anims.count;
            animation = anims.items[anim_index];
            printf("animation name %s\n", animation.name);
        }

        anim_curr_frame = (anim_curr_frame + 1)%animation.key_frames.count;
        update_model_animation(model, anims.items[anim_index], anim_curr_frame);

        begin_drawing(BLUE); {
            begin_mode_3D(camera); {
                draw_model(model);
            } end_mode_3D();

            draw_fps();
        } end_drawing();
    }

    unload_model(model);
    close_window();
    return 0;
}
