#include "../my_computer.h"

#ifdef VULKAN_VALIDATION_ON
    #define VK_VALIDATION 1
#else
    #define VK_VALIDATION 0
#endif

#include "../external/stb_truetype.h"
#include "../external/stb_image.h"

/* engine modules (note: must update nob.c) */
#include "gltf_loader.c"
#include "obj_loader.c"
#include "geometry.c"

#ifndef Z_NEAR
    #define Z_NEAR 0.1
#endif
#ifndef Z_FAR
    #define Z_FAR 500.0
#endif

#ifndef DEFAULT_BITMAP_WIDTH
    #define DEFAULT_BITMAP_WIDTH 400
#endif
#ifndef DEFAULT_BITMAP_HEIGHT
    #define DEFAULT_BITMAP_HEIGHT 400
#endif

#define FPS_CAPTURE_FRAMES_COUNT 30

#ifndef DEFAULT_FONT_SIZE
    #define DEFAULT_FONT_SIZE 42
#endif

#define IMAGE_WORKGROUP_SIZE 16.0f
#define POINT_WORKGROUP_SIZE 1024.0f
#define POINTS_PER_BATCH (1<<22)

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} layers = {0};

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} instance_exts = {0};

static struct {
    const char **items;
    size_t count;
    size_t capacity;
} device_exts = {0};

static Vulkan_Context vk_ctx = {0};
static Frame_Context frame_ctx = {0};

#define MAX_MAT_STACK 1024 * 1024
static Matrix mat_stack[MAX_MAT_STACK];
static size_t mat_stack_p = 0;

static struct {
    Matrix view;
    Matrix proj;
} matrices = {0};

static Camera current_camera = {0};

/* standard descriptor set layouts */
enum {
    DS_LAYOUT_TRIANGLE_RENDER, // TODO: draw_model
    DS_LAYOUT_TEXT,
    DS_LAYOUT_COUNT,
};

enum {
    EYE_LEFT,
    EYE_RIGHT,
    EYE_COUNT,
};

/* standard/default pipelines and buffers */
static struct {
    struct { Rvk_Pipeline pl; } triangle_render; // TODO: draw model
    struct {
        Rvk_Pipeline pl;
        VkDescriptorSet ds;
        Rvk_Texture bitmap;
        Rvk_Buffer index;
        Rvk_Buffer vertex;
        Font default_font;
        String_Builder sb;
    } text;

    Rvk_Buffer uniform_buff;
    struct {
        float16 proj[EYE_COUNT];
        float16 view[EYE_COUNT];
    } uniform_data;

    VkDescriptorSetLayout ds_layouts[DS_LAYOUT_COUNT];

    struct {
        Rvk_Pipeline pl;
        struct {
            int shape_2D;
            int x;
            int y;
            int width;
            int height;
            int window_width;
            int window_height;
            uint32_t color;
        } push_const;
    } primitive_2D;

    Model shapes_3D;

    struct {
        bool disable_render_pass;
    } options;
} standard = {0};

#define MAX_KEYBOARD_KEYS 512
#define MAX_KEY_PRESSED_QUEUE 16
#define MAX_CHAR_PRESSED_QUEUE 16
#define CAMERA_MOVE_SPEED 10.0f
#define CAMERA_MOUSE_MOVE_SENSITIVITY 5.0f
#define CAMERA_ROT_SENSITIVITY 0.1f
#define GAMEPAD_ROT_SENSITIVITY 1.0f
#define MAX_MOUSE_BUTTONS 8
#define MAX_GAMEPAD_BUTTONS 32
#define MAX_GAMEPAD_AXIS 8
#define FPS_CAPTURE_FRAMES_COUNT 30
#define FPS_AVERAGE_TIME_SECONDS 0.5f
#define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)
#define DEAD_ZONE 0.25f

static Vector2 full_screen_quad_verts[] = {
    {-1.0, -1.0},
    { 1.0, -1.0},
    {-1.0,  1.0},
    { 1.0,  1.0},
};

static uint16_t full_screen_quad_indices[] = { 0, 1, 2, 2, 1, 3, };

struct {
    int exit_key;
    char curr_key_state[MAX_KEYBOARD_KEYS];
    char prev_key_state[MAX_KEYBOARD_KEYS];
    char key_repeat_in_frame[MAX_KEYBOARD_KEYS];
    int key_pressed_queue[MAX_KEY_PRESSED_QUEUE];
    int key_pressed_queue_count;
    int char_pressed_queue[MAX_CHAR_PRESSED_QUEUE];
    int char_pressed_queue_count;
} keyboard;

struct {
    Vector2 prev_pos;
    Vector2 curr_pos;
    Vector2 curr_wheel_move;
    Vector2 prev_wheel_move;
    char curr_button_state[MAX_MOUSE_BUTTONS];
    char prev_button_state[MAX_MOUSE_BUTTONS];
} mouse;

struct {
    float axis_state[MAX_GAMEPAD_AXIS];
    char curr_button_state[MAX_GAMEPAD_BUTTONS];
    char prev_button_state[MAX_GAMEPAD_BUTTONS];
    int last_button_pressed;
} gamepad;

static Window window = {0};

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y);
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
static void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
void init_standard_rendering();
void resolve_image();

bool init_window(int width, int height, char *title)
{
    /* initialize glfw and window */
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window.window = glfwCreateWindow(width, height, title, NULL, NULL);
    glfwSetKeyCallback(window.window, key_callback);
    glfwSetMouseButtonCallback(window.window, mouse_button_callback);
    glfwSetCursorPosCallback(window.window, mouse_cursor_pos_callback);
    glfwSetScrollCallback(window.window, mouse_scroll_callback);
    window.width  = width;
    window.height = height;

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_ci = r_get_debug_messenger_info();

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "my computer",
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "my computer engine",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_3,
    };

    if (VK_VALIDATION) {
        da_append(&instance_exts, "VK_EXT_debug_utils");
        da_append(&layers, "VK_LAYER_KHRONOS_validation");
    }

    uint32_t req_ext_count = 0;
    const char **req_inst_exts = glfwGetRequiredInstanceExtensions(&req_ext_count);
    for (uint32_t i = 0; i < req_ext_count; i++) da_append(&instance_exts, req_inst_exts[i]);

    /* create vulkan instance (w/ or w/o validation layers i.e. VK_VALIDATION = 1/0) */
    if (!vk_create_instance(NULL, &vk_ctx.instance,
                            .pNext = (VK_VALIDATION) ? &debug_messenger_ci : NULL,
                            .ppEnabledLayerNames = layers.items,
                            .enabledLayerCount = layers.count,
                            .ppEnabledExtensionNames = instance_exts.items,
                            .enabledExtensionCount = instance_exts.count,
                            .pApplicationInfo = &app_info)) return false;

    /* create the vulkan surface */
    if (!RVK(glfwCreateWindowSurface(vk_ctx.instance, window.window, NULL, &vk_ctx.surface))) return false;

    da_append(&device_exts, "VK_KHR_swapchain");
    Rvk_Device_Config device_config = {
        .extension_count = device_exts.count,
        .extensions = device_exts.items,
        .layer_count = layers.count,
        .layers = layers.items,
    };
    vk_ctx.device = r_create_rvk_device(vk_ctx.instance, vk_ctx.surface, device_config);
    if (!vk_ctx.device.logical) return false;

    /* create swapchain */
    vk_ctx.swapchain = r_create_rvk_swapchain(vk_ctx.device, vk_ctx.surface, width, height);
    if (!vk_ctx.swapchain.handle) return false;

    vk_ctx.pool = r_create_default_descriptor_pool(vk_ctx.device.logical);

    init_standard_rendering();

    return true;
}

void close_window()
{
    vkQueueWaitIdle(vk_ctx.device.queue);

    unload_font(standard.text.default_font);
    destroy_texture(standard.text.bitmap);
    destroy_buffer(standard.text.vertex);
    destroy_buffer(standard.text.index);
    destroy_buffer(standard.uniform_buff);
    destroy_pipeline(standard.triangle_render.pl);
    destroy_pipeline(standard.text.pl);
    destroy_pipeline(standard.primitive_2D.pl);
    destroy_model(standard.shapes_3D);

    for (size_t i = 0; i < DS_LAYOUT_COUNT; i++)
        vkDestroyDescriptorSetLayout(vk_ctx.device.logical, standard.ds_layouts[i], NULL);

    vkDestroyDescriptorPool(vk_ctx.device.logical, vk_ctx.pool, NULL);

    r_destroy_rvk_swapchain(vk_ctx.device.logical, vk_ctx.swapchain);
    r_destroy_rvk_device(vk_ctx.device);
    vkDestroySurfaceKHR(vk_ctx.instance, vk_ctx.surface, NULL);
    vkDestroyInstance(vk_ctx.instance, NULL);
}

bool window_should_close()
{
    bool result = (glfwGetKey(window.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) || glfwWindowShouldClose(window.window);

    glfwPollEvents();

    /* wait for vulkan device to idle so that we can destroy stuff without getting validation warnings */
    if (result) wait_idle();

    return result;
}

Window get_window()
{
    return window;
}

void begin_drawing(Color color)
{
    begin_timer();
    r_wait_reset_fence(vk_ctx.device.logical, &vk_ctx.device.fences[0]);
    r_acquire_next_image(vk_ctx.device.logical, vk_ctx.swapchain.handle,
                         vk_ctx.device.image_available_sems[0], &frame_ctx.img_idx);
    r_reset_begin_cmd_buff(vk_ctx.device.cmd_buffs[0]);

    if (!standard.options.disable_render_pass)
        begin_render_pass(color);
} 

void end_drawing()
{
    if (!standard.options.disable_render_pass)
        end_render_pass();

    /* submit the command buffer and present when ready */
    RVK(vkEndCommandBuffer(vk_ctx.device.cmd_buffs[0]));
    r_submit(vk_ctx.device, 0);
    r_present(vk_ctx.device.queue, vk_ctx.device.render_finished_sems[0], frame_ctx.img_idx, vk_ctx.swapchain.handle);

    end_timer();
    poll_input_events();
}

static void wait_time(double seconds)
{
    if (seconds <= 0) return;

    /* prepare for partial busy wait loop */
    double destination_time = get_time() + seconds;
    double sleep_secs = seconds - seconds * 0.05;

    /* for now wait time only supports linux */
#if defined(__linux__)
    struct timespec req = {0};
    time_t sec = sleep_secs;
    long nsec = (sleep_secs - sec) * 1000000000L;
    req.tv_sec = sec;
    req.tv_nsec = nsec;

    while (nanosleep(&req, &req) == -1) continue;
#endif

#if defined(_WIN32)
    Sleep((unsigned long)(sleep_secs * 1000.0));
#endif

    /* partial busy wait loop */
    while (get_time() < destination_time) {}
}

void begin_timer()
{
    frame_ctx.time.curr   = get_time();
    frame_ctx.time.prev   = (frame_ctx.time.prev == 0) ? frame_ctx.time.curr : frame_ctx.time.prev; // avoid huge time diff on first frame
    frame_ctx.time.update = frame_ctx.time.curr - frame_ctx.time.prev;
    frame_ctx.time.prev   = frame_ctx.time.curr;
}

void end_timer()
{
    frame_ctx.time.curr = get_time();
    frame_ctx.time.draw = frame_ctx.time.curr - frame_ctx.time.prev;
    frame_ctx.time.prev = frame_ctx.time.curr;
    frame_ctx.time.frame = frame_ctx.time.update + frame_ctx.time.draw;

    if (frame_ctx.time.frame < frame_ctx.time.target) {
        wait_time(frame_ctx.time.target - frame_ctx.time.frame);

        frame_ctx.time.curr = get_time();
        double wait = frame_ctx.time.curr - frame_ctx.time.prev;
        frame_ctx.time.prev = frame_ctx.time.curr;
        frame_ctx.time.frame += wait;
    }

    frame_ctx.time.frame_count++;
}

double get_frame_time()
{
    return frame_ctx.time.frame;
}

double get_time()
{
    return glfwGetTime();
}

Vulkan_Context get_vulkan_context()
{
    return vk_ctx;
}

Matrix calc_projection(Camera camera)
{
    double aspect = vk_ctx.swapchain.extent.width / (double)vk_ctx.swapchain.extent.height;
    return MatrixPerspectiveVk(camera.fovy * DEG2RAD, aspect, Z_NEAR, Z_FAR);
}

void begin_mode_3D(Camera camera)
{
    matrices.proj = calc_projection(camera);
    matrices.view = MatrixLookAt(camera.position, camera.target, camera.up);
    current_camera = camera;

    push_matrix();
}

void end_mode_3D()
{
    standard.uniform_data.proj[0] = MatrixToFloatV(get_projection());
    standard.uniform_data.view[0] = MatrixToFloatV(get_view());
    memcpy(standard.uniform_buff.mapped, &standard.uniform_data, sizeof(standard.uniform_data));

    pop_matrix();

    while (mat_stack_p > 0) {
        pop_matrix();
        r_log(RVK_WARNING, "more matrix pushes than pops");
    }
}

void push_matrix()
{
    if (mat_stack_p < MAX_MAT_STACK) {
        if (mat_stack_p) {
            mat_stack[mat_stack_p] = mat_stack[mat_stack_p - 1];
            mat_stack_p++;
        } else {
            mat_stack[mat_stack_p++] = MatrixIdentity();
        }
    } else {
        r_log(RVK_ERROR, "matrix stack overflow");
    }
}

void pop_matrix()
{
    if (mat_stack_p > 0)
        mat_stack_p--;
    else
        r_log(RVK_ERROR, "matrix stack underflow");
}

Matrix get_projection()
{
    return matrices.proj;
}

Matrix get_view()
{
    return matrices.view;
}

Matrix get_model()
{
    Matrix model = MatrixIdentity();
    if (mat_stack_p) model = mat_stack[mat_stack_p - 1];
    return model;
}

Matrix get_mvp()
{
    Matrix model = get_model();
    Matrix view_proj = MatrixMultiply(matrices.view, matrices.proj);
    return MatrixMultiply(model, view_proj);
}

VkCommandBuffer get_current_cmd_buff()
{
    return vk_ctx.device.cmd_buffs[0];
}

VkCommandBuffer get_command_buffer()
{
    return vk_ctx.device.cmd_buffs[0];
}

void set_viewport_scissor()
{
    r_cmd_set_viewport_scissor(vk_ctx.device.cmd_buffs[0], vk_ctx.swapchain.extent);
}

void wait_idle()
{
    vkQueueWaitIdle(vk_ctx.device.queue);
}

Vector3 get_camera_forward(Camera *camera)
{
    return Vector3Normalize(Vector3Subtract(camera->target, camera->position));
}

Vector3 get_camera_up(Camera *camera)
{
    return Vector3Normalize(camera->up);
}

Vector3 get_camera_right(Camera *camera)
{
    Vector3 forward = get_camera_forward(camera);
    Vector3 up = get_camera_up(camera);
    return Vector3CrossProduct(forward, up);
}

void camera_move_forward(Camera *camera, float distance)
{
    Vector3 forward = get_camera_forward(camera);
    forward = Vector3Scale(forward, distance);
    camera->position = Vector3Add(camera->position, forward);
    camera->target = Vector3Add(camera->target, forward);
}

void camera_move_right(Camera *camera, float distance)
{
    Vector3 right = get_camera_right(camera);
    right = Vector3Scale(right, distance);
    camera->position = Vector3Add(camera->position, right);
    camera->target = Vector3Add(camera->target, right);
}

void camera_move_up(Camera *camera, float distance)
{
    Vector3 up = get_camera_up(camera);
    up = Vector3Scale(up, distance);
    camera->position = Vector3Add(camera->position, up);
    camera->target = Vector3Add(camera->target, up);
}

Vector2 get_mouse_delta()
{
    Vector2 delta = {
        .x = mouse.curr_pos.x - mouse.prev_pos.x,
        .y = mouse.curr_pos.y - mouse.prev_pos.y
    };
    return delta;
}

void camera_yaw(Camera *camera, float angle)
{
    Vector3 up = get_camera_up(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, up, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_roll(Camera *camera, float angle)
{
    Vector3 forward = get_camera_forward(camera);
    camera->up = Vector3RotateByAxisAngle(camera->up, forward, angle);
}

void camera_pitch(Camera *camera, float angle)
{
    Vector3 right = get_camera_right(camera);
    Vector3 target_pos = Vector3Subtract(camera->target, camera->position);
    target_pos = Vector3RotateByAxisAngle(target_pos, right, angle);
    camera->target = Vector3Add(camera->position, target_pos);
}

void camera_move_to_target(Camera *camera, float delta)
{
    float distance = Vector3Distance(camera->position, camera->target);
    distance += delta;
    if (distance <= 0) distance = 0.001f;
    Vector3 forward = get_camera_forward(camera);
    camera->position = Vector3Add(camera->target, Vector3Scale(forward, -distance));
}

bool is_key_down(int key)
{
    bool down = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if (keyboard.curr_key_state[key] == 1) down = true;
    }

    return down;
}

bool is_gamepad_button_pressed(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.prev_button_state[button] == 0 && gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

bool is_gamepad_button_down(int button)
{
    bool pressed = false;

    if (button < MAX_GAMEPAD_BUTTONS) {
        if (gamepad.curr_button_state[button] == 1)
            pressed = true;
    }

    return pressed;
}

float get_gamepad_axis_movement(int axis)
{
    float value = 0;

    if (axis < MAX_GAMEPAD_AXIS && fabsf(gamepad.axis_state[axis]) > 0.1f)
        value = gamepad.axis_state[axis];

    return value;
}

int get_mouse_x()
{
    return (int)mouse.curr_pos.x;
}

int get_mouse_y()
{
    return (int)mouse.curr_pos.y;
}

bool is_mouse_button_down(int button)
{
    return mouse.curr_button_state[button] == 1;
}

float get_mouse_wheel_move()
{
    float result = 0.0f;

    if (fabsf(mouse.curr_wheel_move.x) > fabsf(mouse.curr_wheel_move.y)) result = (float)mouse.curr_wheel_move.x;
    else result = (float)mouse.curr_wheel_move.y;

    return result;
}

void update_camera_free(Camera *camera)
{
    Vector2 delta = get_mouse_delta();
    float dt = get_frame_time();

    if (is_mouse_button_down(MOUSE_BUTTON_RIGHT)) {
        camera_yaw(camera,   -delta.x*dt*CAMERA_MOUSE_MOVE_SENSITIVITY);
        camera_pitch(camera, -delta.y*dt*CAMERA_MOUSE_MOVE_SENSITIVITY);
    }

    float move_speed = CAMERA_MOVE_SPEED*dt;

    /* gamepad movement */
    float joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_X);
    float joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_Y);
    float joy_x_norm = (fabsf(joy_x) - DEAD_ZONE)/(1.0f - DEAD_ZONE);
    float joy_y_norm = (fabsf(joy_y) - DEAD_ZONE)/(1.0f - DEAD_ZONE);
    if (joy_x >  DEAD_ZONE) camera_yaw(camera,  -joy_x_norm*dt*GAMEPAD_ROT_SENSITIVITY);
    if (joy_y >  DEAD_ZONE) camera_pitch(camera,-joy_y_norm*dt*GAMEPAD_ROT_SENSITIVITY);
    if (joy_x < -DEAD_ZONE) camera_yaw(camera,   joy_x_norm*dt*GAMEPAD_ROT_SENSITIVITY);
    if (joy_y < -DEAD_ZONE) camera_pitch(camera, joy_y_norm*dt*GAMEPAD_ROT_SENSITIVITY);
    float fb = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
    float lr = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
    float fb_norm = (fabsf(fb) - DEAD_ZONE)/(1.0f - DEAD_ZONE);
    float lr_norm = (fabsf(lr) - DEAD_ZONE)/(1.0f - DEAD_ZONE);
    if (fb <= -DEAD_ZONE) camera_move_forward(camera,  move_speed*fb_norm);
    if (lr <= -DEAD_ZONE) camera_move_right(camera,   -move_speed*lr_norm);
    if (fb >=  DEAD_ZONE) camera_move_forward(camera, -move_speed*fb_norm);
    if (lr >=  DEAD_ZONE) camera_move_right(camera,    move_speed*lr_norm);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) camera_move_up(camera, move_speed/2.0f);
    if (is_gamepad_button_down(GAMEPAD_BUTTON_LEFT_TRIGGER_2))  camera_move_up(camera, -move_speed/2.0f);

    /* keyboard movement */
    if (is_key_down(KEY_LEFT_SHIFT)) move_speed *= 10.0f;
    if (is_key_down(KEY_W)) camera_move_forward(camera,  move_speed);
    if (is_key_down(KEY_A)) camera_move_right(camera,   -move_speed);
    if (is_key_down(KEY_S)) camera_move_forward(camera, -move_speed);
    if (is_key_down(KEY_D)) camera_move_right(camera,    move_speed);
    if (is_key_down(KEY_E)) camera_move_up(camera,  move_speed);
    if (is_key_down(KEY_Q)) camera_move_up(camera, -move_speed);
    if (is_key_down(KEY_LEFT))  camera_roll(camera, -CAMERA_ROT_SENSITIVITY * dt);
    if (is_key_down(KEY_RIGHT)) camera_roll(camera,  CAMERA_ROT_SENSITIVITY * dt);

    // TODO: There should be a threshold check here
    // camera_move_to_target(camera, -get_mouse_wheel_move());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    if (key < 0) return;

    switch (action) {
    case GLFW_RELEASE: keyboard.curr_key_state[key] = 0; break;
    case GLFW_PRESS: keyboard.curr_key_state[key] = 1; break;
    case GLFW_REPEAT: keyboard.key_repeat_in_frame[key] = 1; break;
    }

    if (((key == KEY_CAPS_LOCK) && ((mods & GLFW_MOD_CAPS_LOCK) > 0)) ||
        ((key == KEY_NUM_LOCK) && ((mods & GLFW_MOD_NUM_LOCK) > 0))) keyboard.curr_key_state[key] = 1;

    if ((keyboard.key_pressed_queue_count < MAX_KEY_PRESSED_QUEUE) && (action == GLFW_PRESS)) {
        keyboard.key_pressed_queue[keyboard.key_pressed_queue_count] = key;
        keyboard.key_pressed_queue_count++;
    }

    if ((key == keyboard.exit_key) && (action == GLFW_PRESS)) glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void poll_input_events()
{
    /* keyboard */
    keyboard.key_pressed_queue_count = 0;
    keyboard.char_pressed_queue_count = 0;
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) {
        keyboard.prev_key_state[i] = keyboard.curr_key_state[i];
        keyboard.key_repeat_in_frame[i] = 0;
    }

    /* mouse */
    mouse.prev_pos = mouse.curr_pos;
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        mouse.prev_button_state[i] = mouse.curr_button_state[i];
    mouse.prev_wheel_move = mouse.curr_wheel_move;
    mouse.curr_wheel_move = (Vector2){ 0.0f, 0.0f };

    /* gamepad */
    GLFWgamepadstate state = {0};
    glfwGetGamepadState(0, &state); // 0 means only one controller is supported

    /* gamepad buttons */
    for (int i = 0; i < MAX_GAMEPAD_BUTTONS; i++)
        gamepad.prev_button_state[i] = gamepad.curr_button_state[i];
    const unsigned char *buttons = state.buttons;
    for (int i = 0; buttons != NULL && i < GLFW_GAMEPAD_BUTTON_DPAD_LEFT + 1 && i < MAX_GAMEPAD_BUTTONS; i++) {
        int btn = -1;

        switch (i) {
        case GLFW_GAMEPAD_BUTTON_Y:            btn = GAMEPAD_BUTTON_RIGHT_FACE_UP;    break;
        case GLFW_GAMEPAD_BUTTON_B:            btn = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; break;
        case GLFW_GAMEPAD_BUTTON_A:            btn = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;  break;
        case GLFW_GAMEPAD_BUTTON_X:            btn = GAMEPAD_BUTTON_RIGHT_FACE_LEFT;  break;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:  btn = GAMEPAD_BUTTON_LEFT_TRIGGER_1;   break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: btn = GAMEPAD_BUTTON_RIGHT_TRIGGER_1;  break;
        case GLFW_GAMEPAD_BUTTON_BACK:         btn = GAMEPAD_BUTTON_MIDDLE_LEFT;      break;
        case GLFW_GAMEPAD_BUTTON_GUIDE:        btn = GAMEPAD_BUTTON_MIDDLE;           break;
        case GLFW_GAMEPAD_BUTTON_START:        btn = GAMEPAD_BUTTON_MIDDLE_RIGHT;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP:      btn = GAMEPAD_BUTTON_LEFT_FACE_UP;     break;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:   btn = GAMEPAD_BUTTON_LEFT_FACE_RIGHT;  break;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:    btn = GAMEPAD_BUTTON_LEFT_FACE_DOWN;   break;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:    btn = GAMEPAD_BUTTON_LEFT_FACE_LEFT;   break;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:   btn = GAMEPAD_BUTTON_LEFT_THUMB;       break;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:  btn = GAMEPAD_BUTTON_RIGHT_THUMB;      break;
        default: break;
        }

        if (btn != -1) {
            if (buttons[i] == GLFW_PRESS) {
                gamepad.curr_button_state[btn] = 1;
                gamepad.last_button_pressed = btn;
            } else {
                gamepad.curr_button_state[btn] = 0;
            }
        }
    }

    /* gamepad axes */
    const float *axes = state.axes;
    for (int i = 0; axes != NULL && i < GLFW_GAMEPAD_AXIS_LAST + 1 && i < MAX_GAMEPAD_AXIS; i++)
        gamepad.axis_state[i] = axes[i];

    /* if we want to treat trigger buttons as booleans */
    gamepad.curr_button_state[GAMEPAD_BUTTON_LEFT_TRIGGER_2]  = gamepad.axis_state[GAMEPAD_AXIS_LEFT_TRIGGER] > 0.1f;
    gamepad.curr_button_state[GAMEPAD_BUTTON_RIGHT_TRIGGER_2] = gamepad.axis_state[GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f;

    // glfwPollEvents();
}

void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;
    Vector2 offsets = {.x = x_offset, .y = y_offset};
    mouse.curr_wheel_move = offsets;
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void) window;
    (void) mods;
    mouse.curr_button_state[button] = action;
}

void mouse_cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    (void) window;

    mouse.curr_pos.x = x;
    mouse.curr_pos.y = y;
}

bool is_key_pressed(int key)
{
    bool pressed = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS)) {
        if ((keyboard.prev_key_state[key] == 0) && (keyboard.curr_key_state[key] == 1))
            pressed = true;
    }

    return pressed;
}

void scale(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixScale(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to scale");
}

void translate(float x, float y, float z)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixTranslate(x, y, z), mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to translate");
}

void matrix_cat(Matrix m)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(m, mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to concantenate");
}

int get_fps()
{
    double frame_time = get_frame_time();
    if (frame_time == 0) return 0;
    else return (int)roundf(1.0f / frame_time);
}

int get_avg_fps()
{
    int fps = 0;

    static int index = 0;
    static double history[FPS_CAPTURE_FRAMES_COUNT] = {0};
    static double average = 0, last = 0;
    double fps_frame = get_frame_time();

    if (frame_ctx.time.frame_count == 0) {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fps_frame == 0) return 0;

    if ((get_time() - last) > FPS_STEP)
    {
        last = get_time();
        index = (index + 1) % FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fps_frame / FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    fps = (int)roundf((float)1.0f/average);

    return fps;
}

float get_avg_frame_time()
{
    static int index = 0;
    static double history[FPS_CAPTURE_FRAMES_COUNT] = {0};
    static double average = 0, last = 0;
    double fps_frame = get_frame_time();

    if (frame_ctx.time.frame_count == 0) {
        average = 0;
        last = 0;
        index = 0;

        for (int i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fps_frame == 0) return 0;

    if ((get_time() - last) > FPS_STEP)
    {
        last = get_time();
        index = (index + 1) % FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fps_frame / FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    return average;
}

void log_fps()
{
    static int fps = -1;
    int curr_fps = get_avg_fps();
    if (curr_fps != fps) {
        printf("FPS: %d (%fms)\n", curr_fps, get_avg_frame_time() * 1000.0f);
        fps = curr_fps;
    }
}

Frame_Context get_frame_ctx()
{
    return frame_ctx;
}

VkFramebuffer get_current_frame_buffer()
{
    return vk_ctx.swapchain.framebuffers[frame_ctx.img_idx];
}

Font load_font(const char *file_path, int font_height)
{
    bool result = true;

    Font font = {
        .bitmap_width = DEFAULT_BITMAP_WIDTH,
        .bitmap_height = DEFAULT_BITMAP_HEIGHT,
        .bitmap = malloc(DEFAULT_BITMAP_WIDTH*DEFAULT_BITMAP_HEIGHT),
        .height = font_height,
    };

    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return_defer(false);
    int res = stbtt_BakeFontBitmap((unsigned char *)sb.items, 0, font.height, font.bitmap,
                                   font.bitmap_width, font.bitmap_height, FIRST_CHAR, CHAR_COUNT,
                                   (stbtt_bakedchar*)font.glyphs);
    if (res <= 0) {
        printf("ERROR: unable to fully bake font bitmap (return code: %d)\n", res);
        printf("       if return is negative, returns the negative of the number of characters that fit\n");
        printf("       if return is 0, no characters fit and no rows were used\n");
        printf("       try increasing DEFAULT_BITMAP_WIDTH/HEIGHT in creese_2D.c\n");
        return_defer(false);
    }

defer:
    sb_free(sb);
    if (!result) free(font.bitmap);
    return font;
}

void unload_font(Font font)
{
    free(font.bitmap);
}

// TODO: put inside the standard struct
struct {
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    uint32_t color;
    int window_width;
    int window_height;
} text_push_const;

void create_text_pipeline()
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    Vulkan_Context vk_ctx = get_vulkan_context();
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(text_push_const)
    };

    vk_create_pipeline_layout(vk_ctx.device.logical, NULL, &layout,
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TEXT]);

    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .location = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   =  0,
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vector2),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };

    standard.text.pl = create_triangle_blend_rvk_pipeline("shaders/standard_text.vert.glsl.spv",
                                                          "shaders/standard_text.frag.glsl.spv",
                                                          layout,
                                                          vertex_input_ci);

    /* create and upload quad vertex buffer */
    standard.text.vertex = create_vertex_buffer(sizeof(full_screen_quad_verts), full_screen_quad_verts);
    standard.text.index  = create_index_buffer(sizeof(full_screen_quad_indices), full_screen_quad_indices);
}

void draw_text_at_base(const char *text, size_t text_len, int x, int y, Color color)
{
    if (!standard.text.default_font.bitmap)
        standard.text.default_font = load_font("assets/RobotoMono-Medium.ttf", DEFAULT_FONT_SIZE);
    draw_text_at_base_ex(standard.text.default_font, text, text_len, x, y, color);
}

void draw_text_at_base_ex(Font font, const char *text, size_t text_len, int x, int y, Color color)
{
    if (!standard.text.pl.handle) {
        create_text_pipeline();

        /* allocate and update descriptor sets for text bitmap */
        assert(standard.ds_layouts[DS_LAYOUT_TEXT]);
        if (!vk_allocate_descriptor_sets(vk_ctx.device.logical, &standard.text.ds,
                                         .descriptorPool = vk_ctx.pool,
                                         .descriptorSetCount = 1,
                                         .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TEXT])) return;

        VkExtent2D extent = {.width = font.bitmap_width, .height = font.bitmap_height};
        standard.text.bitmap = r_create_texture(vk_ctx.device, extent, font.bitmap, VK_FORMAT_R8_UNORM);
        assert(standard.text.ds);
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .dstSet = standard.text.ds,
            .pImageInfo = &standard.text.bitmap.info,
        };
        vkUpdateDescriptorSets(vk_ctx.device.logical, 1, &write, 0, NULL);
    }

    VkCommandBuffer cb = vk_ctx.device.cmd_buffs[0];
    vkCmdBindPipeline(cb, 0, standard.text.pl.handle);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, standard.text.pl.layout,
                            0, 1, &standard.text.ds, 0, NULL);
    r_cmd_set_viewport_scissor(cb, vk_ctx.swapchain.extent);

    int x_advance = 0;
    for (size_t c = 0; c < text_len; c++) {
        uint8_t ch = text[c];
        if (!(FIRST_CHAR <= ch && ch < (CHAR_COUNT + FIRST_CHAR))) continue;
        Glyph glyph = font.glyphs[ch-FIRST_CHAR];
        text_push_const.color = color_to_uint32_t(color);
        text_push_const.x = x + x_advance + glyph.x_offset;
        text_push_const.y = y + glyph.y_offset;
        text_push_const.x0 = glyph.x0;
        text_push_const.y0 = glyph.y0;
        text_push_const.x1 = glyph.x1;
        text_push_const.y1 = glyph.y1;
        text_push_const.window_width = window.width;
        text_push_const.window_height= window.height;

        vkCmdPushConstants(cb, standard.text.pl.layout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(text_push_const), &text_push_const);
        r_cmd_draw_buffers(cb, standard.text.vertex.info.buffer, standard.text.index.info.buffer, ARRAY_LEN(full_screen_quad_indices));

        x_advance += glyph.x_advance;
    }
}

uint32_t color_to_uint32_t(Color color)
{
    uint32_t r = color.r;
    uint32_t g = color.g;
    uint32_t b = color.b;
    uint32_t a = color.a;
    return a<<24 | b<<16 | g<<8 | r;
}

Color uint32_t_to_color(uint32_t rgba)
{
    Color c = {0};
    uint32_t r = (rgba>> 0)&0xff;
    uint32_t g = (rgba>> 8)&0xff;
    uint32_t b = (rgba>>16)&0xff;
    uint32_t a = (rgba>>24)&0xff;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

Color color_from_hsv(float hue, float saturation, float value)
{
    Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}

VkPipelineShaderStageCreateInfo load_compute_shader(const char *file_path, String_Builder *sb)
{
    VkPipelineShaderStageCreateInfo ci = {0};

    sb->count = 0;
    if (read_entire_file(file_path, sb))
        ci = r_create_compute_shader_stage_ci(vk_ctx.device.logical, sb->count, (uint32_t*)sb->items);

    return ci;
}

void unload_shader(VkPipelineShaderStageCreateInfo ci)
{
    vkDestroyShaderModule(vk_ctx.device.logical, ci.module, NULL);
}

void destroy_pipeline(Rvk_Pipeline pipeline)
{
    vkDestroyPipeline(vk_ctx.device.logical, pipeline.handle, NULL);
    vkDestroyPipelineLayout(vk_ctx.device.logical, pipeline.layout, NULL);
}

void setup_required_standard_ds_layouts()
{
    /* standard_triangle_render.vert.glsl */
    VkDescriptorSetLayoutBinding tri_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 3,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 4,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 5,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 6,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding         = 7,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding         = 8,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding         = 9,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    vk_create_descriptor_set_layout(vk_ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_TRIANGLE_RENDER],
                                    .pBindings = tri_bindings,
                                    .bindingCount = ARRAY_LEN(tri_bindings));

    /* standard text */
    VkDescriptorSetLayoutBinding text_bindings[] = {
        {
            .binding         = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    vk_create_descriptor_set_layout(vk_ctx.device.logical, NULL, &standard.ds_layouts[DS_LAYOUT_TEXT],
                                    .pBindings = text_bindings,
                                    .bindingCount = ARRAY_LEN(text_bindings));

}

void init_standard_rendering()
{
    setup_required_standard_ds_layouts();
    standard.uniform_buff = r_create_mapped_uniform_buffer(vk_ctx.device, sizeof(standard.uniform_data));
}

Rvk_Buffer create_compute_buffer(size_t size, void *data)
{
    return r_create_compute_buffer_from_host(vk_ctx.device, size, data);
}

void destroy_buffer(Rvk_Buffer buff)
{
    r_destroy_rvk_buffer(vk_ctx.device.logical, buff);
}

void destroy_texture(Rvk_Texture texture)
{
    r_destroy_texture(vk_ctx.device.logical, texture);
}

Rvk_Buffer create_vertex_buffer(size_t size, void *vertices)
{
    return r_create_vertex_buffer(vk_ctx.device, size, vertices);
}

Rvk_Buffer create_index_buffer(size_t size, void *indices)
{
    return r_create_index_buffer(vk_ctx.device, size, indices);
}

Rvk_Pipeline create_triangle_rvk_pipeline(const char *vert_shader, const char *frag_shader, VkPipelineLayout layout, VkPipelineVertexInputStateCreateInfo vert_input)
{
    Rvk_Pipeline pl = {.layout = layout};
    VkPipelineShaderStageCreateInfo stages[2];
    String_Builder sb = {0};

    /* load shaders */
    read_entire_file(vert_shader, &sb);
    stages[0] = r_create_vertex_stage_ci(vk_ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[0].module);
    sb.count = 0; // reuse memory
    read_entire_file(frag_shader, &sb);
    stages[1] = r_create_fragment_stage_ci(vk_ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[1].module);
    sb.count = 0;

    VkPipelineVertexInputStateCreateInfo *p_vert_input = NULL;
    if (vert_input.sType != VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO)
        p_vert_input = r_temp_default_empty_input_state_ci();
    else
        p_vert_input = &vert_input;

    /* temporary allocator */
    size_t temp_alloc_save_point = r_temp_save();
    vk_create_graphics_pipeline(vk_ctx.device.logical, NULL, NULL, &pl.handle,
                                .stageCount = ARRAY_LEN(stages),
                                .pStages = stages,
                                .pVertexInputState = p_vert_input,
                                .pInputAssemblyState = r_temp_default_input_assembly_state_ci(),
                                .pViewportState = r_temp_default_viewport_state_ci(vk_ctx.swapchain.extent),
                                .pRasterizationState = r_temp_default_rasterization_state_ci(),
                                .pMultisampleState = r_temp_default_multisample_state_ci(),
                                .pDepthStencilState = r_temp_default_depth_stencil_state_ci(),
                                .pColorBlendState = r_temp_default_color_blend_state_ci(),
                                .pDynamicState = r_temp_default_dynamic_state_ci(),
                                .layout = pl.layout,
                                .renderPass = vk_ctx.swapchain.render_pass);
    r_temp_rewind(temp_alloc_save_point);

    vkDestroyShaderModule(vk_ctx.device.logical, stages[0].module, NULL);
    vkDestroyShaderModule(vk_ctx.device.logical, stages[1].module, NULL);
    sb_free(sb);

    return pl;
}

Rvk_Pipeline create_triangle_blend_rvk_pipeline(const char *vert_shader, const char *frag_shader, VkPipelineLayout layout, VkPipelineVertexInputStateCreateInfo vert_input)
{
    Rvk_Pipeline pl = {.layout = layout};
    VkPipelineShaderStageCreateInfo stages[2];
    String_Builder sb = {0};

    /* load shaders */
    read_entire_file(vert_shader, &sb);
    stages[0] = r_create_vertex_stage_ci(vk_ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[0].module);
    sb.count = 0; // reuse memory
    read_entire_file(frag_shader, &sb);
    stages[1] = r_create_fragment_stage_ci(vk_ctx.device.logical, sb.count, (uint32_t*)sb.items);
    assert(stages[1].module);
    sb.count = 0;

    /* color blend */
    VkPipelineColorBlendAttachmentState color_blend = {
        .colorWriteMask = 0xf, // rgba
        .blendEnable = VK_TRUE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend,
        .logicOp = VK_LOGIC_OP_COPY,
    };


    /* temporary allocator */
    size_t temp_alloc_save_point = r_temp_save();
    vk_create_graphics_pipeline(vk_ctx.device.logical, NULL, NULL, &pl.handle,
                                .stageCount = ARRAY_LEN(stages),
                                .pStages = stages,
                                .pVertexInputState = &vert_input,
                                .pInputAssemblyState = r_temp_default_input_assembly_state_ci(),
                                .pViewportState = r_temp_default_viewport_state_ci(vk_ctx.swapchain.extent),
                                .pRasterizationState = r_temp_default_rasterization_state_ci(),
                                .pMultisampleState = r_temp_default_multisample_state_ci(),
                                .pDepthStencilState = r_temp_default_depth_stencil_state_ci(),
                                .pColorBlendState = &color_blend_ci,
                                .pDynamicState = r_temp_default_dynamic_state_ci(),
                                .layout = pl.layout,
                                .renderPass = vk_ctx.swapchain.render_pass);
    r_temp_rewind(temp_alloc_save_point);

    vkDestroyShaderModule(vk_ctx.device.logical, stages[0].module, NULL);
    vkDestroyShaderModule(vk_ctx.device.logical, stages[1].module, NULL);
    sb_free(sb);

    return pl;
}

void begin_render_pass(Color color)
{
    float r = color.r/255.0f;
    float g = color.g/255.0f;
    float b = color.b/255.0f;
    float a = color.a/255.0f;

    VkClearValue clear_color = {
        .color = {
            {r, g, b, a}
        }
    };
    VkClearValue clear_depth = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0,
        }
    };
    VkClearValue clear_values[] = {clear_color, clear_depth};
    VkExtent2D extent = {window.width, window.height};
    VkFramebuffer fb = get_current_frame_buffer();
    VkRenderPassBeginInfo begin_rp = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = vk_ctx.swapchain.render_pass,
        .framebuffer = fb,
        .renderArea.extent = extent,
        .clearValueCount = ARRAY_LEN(clear_values),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(vk_ctx.device.cmd_buffs[0], &begin_rp, VK_SUBPASS_CONTENTS_INLINE);
}

void end_render_pass()
{
    vkCmdEndRenderPass(vk_ctx.device.cmd_buffs[0]);
}

void bind_graphics_pipeline(VkPipeline pl)
{
    VkCommandBuffer cmd_buff = vk_ctx.device.cmd_buffs[0];
    vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, pl);
}

void rotate_y(float angle)
{
    if (mat_stack_p > 0)
        mat_stack[mat_stack_p - 1] = MatrixMultiply(MatrixRotateY(angle), mat_stack[mat_stack_p - 1]);
    else
        r_log(RVK_ERROR, "no matrix available to rotate y");
}

struct {
    float16 model;
    uint32_t color;
    uint32_t attribute_mask;
    uint32_t material_mask;
} tri_render_push_const;

void init_tri_model_pipeline()
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    Vulkan_Context vk_ctx = get_vulkan_context();
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(tri_render_push_const)
    };

    vk_create_pipeline_layout(vk_ctx.device.logical, NULL, &layout,
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range,
                              .setLayoutCount = 1,
                              .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TRIANGLE_RENDER]);

    VkVertexInputAttributeDescription vert_attrs[] = {
        {
            .location = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   =  0,
        },
    };
    VkVertexInputBindingDescription vert_bindings = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride    = sizeof(Vector3),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vert_bindings,
        .vertexAttributeDescriptionCount = ARRAY_LEN(vert_attrs),
        .pVertexAttributeDescriptions = vert_attrs,
    };

    standard.triangle_render.pl = create_triangle_rvk_pipeline("shaders/standard_triangle_render.vert.glsl.spv",
                                                               "shaders/standard_triangle_render.frag.glsl.spv",
                                                               layout,
                                                               vertex_input_ci);
}

void destroy_model(Model model)
{
    for (size_t i = 0; i < model.meshes.count; i++) {
        Mesh mesh = model.meshes.items[i];
        da_free(mesh.cpu.positions);
        da_free(mesh.cpu.normals);
        da_free(mesh.cpu.uvs);
        da_free(mesh.cpu.tangets);
        da_free(mesh.cpu.colors);
        da_free(mesh.cpu.indices);

        destroy_buffer(mesh.gpu.vertex);
        destroy_buffer(mesh.gpu.index);

        /* we check before destroying here because a phony buffer might be used and we don't want to
         * destroy that twice */
        if (mesh.gpu.mask & 1<<ATTRIBUTE_NORMAL) destroy_buffer(mesh.gpu.normal);
        if (mesh.gpu.mask & 1<<ATTRIBUTE_UV)     destroy_buffer(mesh.gpu.uv);
        if (mesh.gpu.mask & 1<<ATTRIBUTE_TANGET) destroy_buffer(mesh.gpu.tanget);
        if (mesh.gpu.mask & 1<<ATTRIBUTE_COLOR)  destroy_buffer(mesh.gpu.color);
        if (mesh.gpu.mask & 1<<ATTRIBUTE_JOINT)  destroy_buffer(mesh.gpu.joint);
        if (mesh.gpu.mask & 1<<ATTRIBUTE_WEIGHT) destroy_buffer(mesh.gpu.weight);
    }

    for (size_t i = 0; i < model.textures.count; i++) {
        free(model.images.items[i].data);
        destroy_texture(model.textures.items[i]);
    }

    destroy_texture(model.phony.texture);
    destroy_buffer(model.phony.buffer);
}

void load_model_gpu(Model *model)
{
    /* create phony attribute buffer and phony texture */
    model->phony.buffer  = create_compute_buffer(sizeof(model->phony.data), &model->phony.data);
    model->phony.image   = (Creese_Image){.width = 1, .height = 1, .data = &model->phony.data, };
    model->phony.texture = create_texture(model->phony.image);

    /* create textures */
    for (size_t i = 0; i < model->images.count; i++) {
        Rvk_Texture texture = create_texture(model->images.items[i]);
        da_append(&model->textures, texture);
    }

    /* create compute buffers for the attributes, and also textures */
    for (size_t i = 0; i < model->meshes.count; i++) {
        Mesh *mesh = &model->meshes.items[i];

        /* vertex and index buffers are required, everything else is optional */
        size_t size = mesh->cpu.positions.count*sizeof(*mesh->cpu.positions.items);
        assert(size);
        mesh->gpu.vertex = create_vertex_buffer(size, mesh->cpu.positions.items);
        size = mesh->cpu.indices.count*sizeof(*mesh->cpu.indices.items);
        assert(size);
        mesh->gpu.index = create_index_buffer(size, mesh->cpu.indices.items);

        size = mesh->cpu.normals.count*sizeof(*mesh->cpu.normals.items);
        if (size) {
            assert(mesh->gpu.normal.info.buffer == NULL);
            mesh->gpu.normal = create_compute_buffer(size, mesh->cpu.normals.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_NORMAL;
        } else mesh->gpu.normal = model->phony.buffer;

        size = mesh->cpu.uvs.count*sizeof(*mesh->cpu.uvs.items);
        if (size) {
            assert(mesh->gpu.uv.info.buffer == NULL);
            mesh->gpu.uv = create_compute_buffer(size, mesh->cpu.uvs.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_UV;
        } else mesh->gpu.uv = model->phony.buffer;

        size = mesh->cpu.colors.count*sizeof(*mesh->cpu.colors.items);
        if (size) {
            assert(mesh->gpu.color.info.buffer == NULL);
            mesh->gpu.color = create_compute_buffer(size, mesh->cpu.colors.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_COLOR;
        } else mesh->gpu.color = model->phony.buffer;

        size = mesh->cpu.tangets.count*sizeof(*mesh->cpu.tangets.items);
        if (size) {
            assert(mesh->gpu.tanget.info.buffer == NULL);
            mesh->gpu.tanget = create_compute_buffer(size, mesh->cpu.tangets.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_TANGET;
        } else mesh->gpu.tanget = model->phony.buffer;

        size = mesh->cpu.joints.count*sizeof(*mesh->cpu.joints.items);
        if (size) {
            assert(mesh->gpu.joint.info.buffer == NULL);
            mesh->gpu.joint = create_compute_buffer(size, mesh->cpu.joints.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_JOINT;
        } else mesh->gpu.joint = model->phony.buffer;

        size = mesh->cpu.weights.count*sizeof(*mesh->cpu.weights.items);
        if (size) {
            assert(mesh->gpu.weight.info.buffer == NULL);
            mesh->gpu.weight = create_compute_buffer(size, mesh->cpu.weights.items);
            mesh->gpu.mask |= 1<<ATTRIBUTE_WEIGHT;
        } else mesh->gpu.weight = model->phony.buffer;


        /* get the image info */
        VkDescriptorImageInfo *albedo_image_info = NULL;
        VkDescriptorImageInfo *normal_image_info = NULL;
        VkDescriptorImageInfo *metallic_roughness_image_info = NULL;
        if (mesh->material.mask & 1<<MATERIAL_ALBEDO)
            albedo_image_info = &model->textures.items[mesh->material.albedo_image_index].info;
        else
            albedo_image_info = &model->phony.texture.info;
        if (mesh->material.mask & 1<<MATERIAL_NORMAL)
            normal_image_info = &model->textures.items[mesh->material.normal_image_index].info;
        else
            normal_image_info = &model->phony.texture.info;
        if (mesh->material.mask & 1<<MATERIAL_METALLIC_ROUGHNESS)
            metallic_roughness_image_info = &model->textures.items[mesh->material.metallic_roughness_image_index].info;
        else
            metallic_roughness_image_info = &model->phony.texture.info;

        /* allocate descriptor set */
        assert(standard.ds_layouts[DS_LAYOUT_TRIANGLE_RENDER]);
        vk_allocate_descriptor_sets(vk_ctx.device.logical, &mesh->gpu.ds,
                                    .descriptorPool = vk_ctx.pool,
                                    .descriptorSetCount = 1,
                                    .pSetLayouts = &standard.ds_layouts[DS_LAYOUT_TRIANGLE_RENDER]);

        /* update descriptor set */
        VkWriteDescriptorSet writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &standard.uniform_buff.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.normal.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.uv.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.color.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.tanget.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 5,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.joint.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 6,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pBufferInfo = &mesh->gpu.weight.info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 7,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pImageInfo = albedo_image_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 8,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pImageInfo = normal_image_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = 9,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .dstSet = mesh->gpu.ds,
                .pImageInfo = metallic_roughness_image_info,
            },
        };
        vkUpdateDescriptorSets(vk_ctx.device.logical, ARRAY_LEN(writes), writes, 0, NULL);
    }


}

void draw_model(Model model)
{
    if (!standard.triangle_render.pl.handle) init_tri_model_pipeline();

    VkCommandBuffer cb = get_command_buffer();
    bind_graphics_pipeline(standard.triangle_render.pl.handle);
    set_viewport_scissor();

    /* TODO: at some point we may want to get the model matrix from scene data */
    tri_render_push_const.model = MatrixToFloatV(get_model());

    for (size_t i = 0; i < model.meshes.count; i++) {
        Mesh mesh = model.meshes.items[i];

        tri_render_push_const.attribute_mask = mesh.gpu.mask;
        tri_render_push_const.material_mask = mesh.material.mask;
        uint32_t color = color_to_uint32_t(mesh.material.color);
        uint32_t default_color = color_to_uint32_t(MAGENTA);
        tri_render_push_const.color = color ? color : default_color;
        VkShaderStageFlags flags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
        vkCmdPushConstants(cb, standard.triangle_render.pl.layout, flags, 0, sizeof(tri_render_push_const), &tri_render_push_const);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                standard.triangle_render.pl.layout, 0, 1,
                                &mesh.gpu.ds, 0, NULL);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cb, 0, 1, &mesh.gpu.vertex.info.buffer, offsets);
        vkCmdBindIndexBuffer(cb, mesh.gpu.index.info.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, mesh.cpu.indices.count, 1, 0, 0, 0);
    }
}

void draw_mesh(Mesh mesh)
{
    if (!standard.triangle_render.pl.handle) init_tri_model_pipeline();

    VkCommandBuffer cb = get_command_buffer();
    bind_graphics_pipeline(standard.triangle_render.pl.handle);
    set_viewport_scissor();

    /* at some point we may want to get the model matrix from scene data */
    tri_render_push_const.model = MatrixToFloatV(get_model());

    tri_render_push_const.attribute_mask = mesh.gpu.mask;
    tri_render_push_const.material_mask = mesh.material.mask;
    uint32_t color = color_to_uint32_t(mesh.material.color);
    uint32_t default_color = color_to_uint32_t(MAGENTA);
    tri_render_push_const.color = color ? color : default_color;
    VkShaderStageFlags flags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    vkCmdPushConstants(cb, standard.triangle_render.pl.layout, flags, 0, sizeof(tri_render_push_const), &tri_render_push_const);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            standard.triangle_render.pl.layout, 0, 1,
                            &mesh.gpu.ds, 0, NULL);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cb, 0, 1, &mesh.gpu.vertex.info.buffer, offsets);
    vkCmdBindIndexBuffer(cb, mesh.gpu.index.info.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cb, mesh.cpu.indices.count, 1, 0, 0, 0);
}

Creese_Image create_image(const char *file_name)
{
    Creese_Image img = {0};
    int channels;
    img.data = stbi_load(file_name, &img.width, &img.height, &channels, STBI_rgb_alpha);
    if (!img.data) printf("ERROR: failed to load image %s\n", file_name);
    return img;
}

void disable_render_pass()
{
    standard.options.disable_render_pass = true;
}

Rvk_Texture create_texture(Creese_Image img)
{
    VkFormat format = 0;
    VkExtent2D extent = {img.width, img.height};

    switch (img.type) {
    case IMAGE_TYPE_UNORM: format = VK_FORMAT_R8G8B8A8_UNORM; break;
    case IMAGE_TYPE_SRGB:  format = VK_FORMAT_R8G8B8A8_SRGB;  break;
    default:               format = VK_FORMAT_R8G8B8A8_SRGB;  break;
    }

    return r_create_texture(vk_ctx.device, extent, img.data, format);
}

void init_standard_3D_shapes()
{
    /* cube */
    Mesh mesh = {0};
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&mesh.cpu.positions, cube_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&mesh.cpu.normals, cube_verts[i].normal);
        da_append(&mesh.cpu.indices, i);
    }
    da_append(&standard.shapes_3D.meshes, mesh);

    /* tetrahedron */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(tetrahedron_verts); i++) {
        da_append(&mesh.cpu.positions, tetrahedron_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(tetrahedron_verts[i].color));
    }
    for (size_t i = 0; i < ARRAY_LEN(tetrahedron_indices); i++)
        da_append(&mesh.cpu.indices, tetrahedron_indices[i]);
    da_append(&standard.shapes_3D.meshes, mesh);

    /* quad */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(quad_verts); i++) {
        da_append(&mesh.cpu.positions, quad_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(quad_verts[i].color));
    }
    for (size_t i = 0; i < ARRAY_LEN(quad_indices); i++)
        da_append(&mesh.cpu.indices, quad_indices[i]);
    da_append(&standard.shapes_3D.meshes, mesh);

    /* triangle */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(triangle_verts); i++) {
        da_append(&mesh.cpu.positions, triangle_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(triangle_verts[i].color));
        da_append(&mesh.cpu.indices, i);
    }
    da_append(&standard.shapes_3D.meshes, mesh);

    load_model_gpu(&standard.shapes_3D);
}

void draw_shape_3D(Shape_3D shape)
{
    if (!standard.shapes_3D.meshes.count) init_standard_3D_shapes();
    if (shape >= 0 && shape < SHAPE_3D_COUNT)
        draw_mesh(standard.shapes_3D.meshes.items[shape]);
}

void draw_fps()
{
    if (!standard.text.default_font.bitmap)
        standard.text.default_font = load_font("assets/RobotoMono-Medium.ttf", DEFAULT_FONT_SIZE);

    standard.text.sb.count = 0;
    int fps = get_avg_fps();
    sb_appendf(&standard.text.sb, "FPS:%d", fps);
    Color color = GREEN;
    if (fps < 30 && fps >= 15) color = ORANGE;
    else if (fps < 15) color = RED;
    draw_text_at_base_ex(standard.text.default_font, standard.text.sb.items, standard.text.sb.count, 20, 42, color);
}

int measure_text(const char *text, size_t text_len)
{
    if (!standard.text.default_font.bitmap)
        standard.text.default_font = load_font("assets/RobotoMono-Medium.ttf", DEFAULT_FONT_SIZE);

    return measure_text_ex(standard.text.default_font, text, text_len);
}

int measure_text_ex(Font font, const char *text, size_t text_len)
{
    size_t width = 0;
    for (size_t i = 0; i < text_len; i++) {
        uint8_t ch = text[i];
        if (!(FIRST_CHAR <= ch && ch < (CHAR_COUNT + FIRST_CHAR))) continue;
        width += font.glyphs[ch-FIRST_CHAR].x_advance;
    }

    return width;
}

void init_primitive_2D_pipeline()
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(standard.primitive_2D.push_const)
    };

    vk_create_pipeline_layout(vk_ctx.device.logical, NULL, &layout,
                              .pushConstantRangeCount = 1,
                              .pPushConstantRanges = &pc_range);

    VkPipelineVertexInputStateCreateInfo empty_vertex_input_ci = {0};
    standard.primitive_2D.pl = create_triangle_rvk_pipeline("shaders/primitive_2D.vert.glsl.spv",
                                                            "shaders/primitive_2D.frag.glsl.spv",
                                                            layout,
                                                            empty_vertex_input_ci);
}

void draw_rectangle(int x, int y, int width, int height, Color color)
{
    if (!standard.primitive_2D.pl.handle) init_primitive_2D_pipeline();

    Window window = get_window();
    standard.primitive_2D.push_const.shape_2D = SHAPE_2D_RECTANGLE;
    standard.primitive_2D.push_const.x = x;
    standard.primitive_2D.push_const.y = y;
    standard.primitive_2D.push_const.width = width;
    standard.primitive_2D.push_const.height = height;
    standard.primitive_2D.push_const.window_width  = window.width;
    standard.primitive_2D.push_const.window_height = window.height;
    standard.primitive_2D.push_const.color = color_to_uint32_t(color);

    VkCommandBuffer cb = get_command_buffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, standard.primitive_2D.pl.handle);
    VkShaderStageFlags flags = VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_VERTEX_BIT;
    size_t size = sizeof(standard.primitive_2D.push_const);
    vkCmdPushConstants(cb, standard.primitive_2D.pl.layout, flags, 0, size, &standard.primitive_2D.push_const);
    set_viewport_scissor();
    vkCmdDraw(cb, 6, 1, 0, 0);
}

void draw_circle(int x, int y, int radius, Color color)
{
    if (!standard.primitive_2D.pl.handle) init_primitive_2D_pipeline();

    Window window = get_window();
    standard.primitive_2D.push_const.shape_2D = SHAPE_2D_CIRCLE;
    standard.primitive_2D.push_const.width  = radius*2;
    standard.primitive_2D.push_const.height = radius*2;
    standard.primitive_2D.push_const.x = x - standard.primitive_2D.push_const.width /2;
    standard.primitive_2D.push_const.y = y - standard.primitive_2D.push_const.height/2;
    standard.primitive_2D.push_const.window_width  = window.width;
    standard.primitive_2D.push_const.window_height = window.height;
    standard.primitive_2D.push_const.color = color_to_uint32_t(color);

    VkCommandBuffer cb = get_command_buffer();
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, standard.primitive_2D.pl.handle);
    VkShaderStageFlags flags = VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_VERTEX_BIT;
    size_t size = sizeof(standard.primitive_2D.push_const);
    vkCmdPushConstants(cb, standard.primitive_2D.pl.layout, flags, 0, size, &standard.primitive_2D.push_const);
    set_viewport_scissor();
    vkCmdDraw(cb, 6, 1, 0, 0);
}
