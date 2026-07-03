#include "my_computer.h"

/*
 *      1---------3
 *    / |        /|
 *   0----------2 |
 *   |  |       | |
 *   |  |       | |
 *   |  5-------|-7
 *   |/         |/
 *   4----------6
 *
 */

#define V0 {-0.5f,  0.5f,  0.5f}
#define V1 {-0.5f,  0.5f, -0.5f}
#define V2 { 0.5f,  0.5f,  0.5f}
#define V3 { 0.5f,  0.5f, -0.5f}
#define V4 {-0.5f, -0.5f,  0.5f}
#define V5 {-0.5f, -0.5f, -0.5f}
#define V6 { 0.5f, -0.5f,  0.5f}
#define V7 { 0.5f, -0.5f, -0.5f}
#define N_UP       { 0.0f,  1.0f,  0.0f}
#define N_DOWN     { 0.0f, -1.0f,  0.0f}
#define N_LEFT     {-1.0f,  0.0f,  0.0f}
#define N_RIGHT    { 1.0f,  0.0f,  0.0f}
#define N_FORWARD  { 0.0f,  0.0f, -1.0f}
#define N_BACKWARD { 0.0f,  0.0f,  1.0f}

static struct {
    Vector3 position;
    Vector3 normal;
    Color color;
} cube_verts[] = {
    // TOP
    { .position = V0, .color = PINK, .normal = N_UP,},
    { .position = V1, .color = PINK, .normal = N_UP,},
    { .position = V2, .color = PINK, .normal = N_UP,},
    { .position = V2, .color = PINK, .normal = N_UP,},
    { .position = V1, .color = PINK, .normal = N_UP,},
    { .position = V3, .color = PINK, .normal = N_UP,},
    // FRONT
    { .position = V0, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V2, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V4, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V4, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V2, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V6, .color = BLUE, .normal = N_BACKWARD,},
    // RIGHT
    { .position = V6, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V2, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V3, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V6, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V3, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V7, .color = VIOLET, .normal = N_RIGHT,},
    // BACK
    { .position = V7, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V3, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V5, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V5, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V3, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V1, .color = DARKGREEN, .normal = N_FORWARD},
    // LEFT
    { .position = V5, .color = GOLD, .normal = N_LEFT,},
    { .position = V1, .color = GOLD, .normal = N_LEFT,},
    { .position = V0, .color = GOLD, .normal = N_LEFT,},
    { .position = V5, .color = GOLD, .normal = N_LEFT,},
    { .position = V0, .color = GOLD, .normal = N_LEFT,},
    { .position = V4, .color = GOLD, .normal = N_LEFT,},
    // BOTTOM
    { .position = V5, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V4, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V6, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V5, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V6, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V7, .color = MAGENTA, .normal = N_DOWN,},
};

static struct {
    Vector3 position;
    Color color;
} triangle_verts[] = {
    {.position = {-0.5, -0.5, 0.0}, .color = RED},
    {.position = { 0.5, -0.5, 0.0}, .color = GREEN},
    {.position = { 0.0,  0.5, 0.0}, .color = BLUE},
};

static struct {
    Vector3 position;
    Color color;
} tetrahedron_verts[] = {
    {.position = {0.0f, -0.333f, 0.943f},     .color = GOLD},
    {.position = {0.816f, -0.333f, -0.471f},  .color = MAGENTA},
    {.position = {-0.816f, -0.333f, -0.471f}, .color = DARKGREEN},
    {.position = {0.0f, 1.0f, 0.0f},          .color = VIOLET},
};

static uint32_t tetrahedron_indices[] = {
    0, 3, 1,
    0, 2, 3,
    0, 1, 2,
    3, 2, 1,
};

int main()
{
    init_window(1600, 900, "cube");

    Model cube = {0};
    // Model triangle = {0};
    // Model tetrahedron = {0};
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&cube.host_mem.positions, cube_verts[i].position);
        da_append(&cube.host_mem.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&cube.host_mem.normals, cube_verts[i].normal);
        da_append(&cube.host_mem.indices, i);
        // da_append(&triangle.host_mem.positions, triangle_verts[i].position);
        // da_append(&triangle.host_mem.colors, color_to_uint32_t(triangle_verts[i].color));
        // da_append(&triangle.host_mem.indices, i);
        // da_append(&tetrahedron.host_mem.positions, tetrahedron_verts[i].position);
        // da_append(&tetrahedron.host_mem.colors, color_to_uint32_t(tetrahedron_verts[i].color));
    }
    // for (size_t i = 0; i < ARRAY_LEN(tetrahedron_indices); i++)
    //     da_append(&tetrahedron.host_mem.indices, tetrahedron_indices[i]);

    load_model_gpu(&cube);
    // load_model_gpu(&triangle);
    // load_model_gpu(&tetrahedron);

    Model bunny = load_model_from_obj("assets/bunny.obj");

    Camera camera = {
        .position = {0.0f, 2.0f, 5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };


    while (!window_should_close()) {
        if (is_key_down(KEY_F)) log_fps();

        update_camera_free(&camera);

        begin_drawing();
            begin_render_pass(BLACK);
                begin_mode_3D(camera);
                    rotate_y(get_time());
                    // draw_model(bunny);
                    draw_model(cube);
                end_mode_3D();
            end_render_pass();
        end_drawing();
    }

    destroy_model(cube);
    // destroy_model(triangle);
    // destroy_model(tetrahedron);
    destroy_model(bunny);

    close_window();

    return 0;
}
