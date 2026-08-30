#include "my_computer.h"

typedef struct {
    struct {
        int x, y;
    } position;
    int width, height;
    bool draw;

    struct {
        float timer;
        float threshold;
    } blink;
} Cursor;

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_COLS 100
#define MAX_ROWS 100
char text[MAX_COLS][MAX_ROWS] = {0};

int main()
{
    init_window(WINDOW_WIDTH, WINDOW_HEIGHT, "text editing");

    for (int y = 0; y < MAX_ROWS; y++)
        for (int x = 0; x < MAX_ROWS; x++)
            text[y][x] = 32;

    Cursor cursor = {
        .width = 27,
        .height = 42,
        .draw = true,
        .blink = {.threshold = 0.5},
    };

    while (!window_should_close()) {
        if (is_key_pressed(KEY_RIGHT)) {
            cursor.draw = true;
            cursor.blink.timer = 0;
            int nx = cursor.position.x + 1;
            if (!(nx*cursor.width > WINDOW_WIDTH)) {
                cursor.position.x = nx;
            }
        }
        if (is_key_pressed(KEY_LEFT)) {
            cursor.draw = true;
            cursor.blink.timer = 0;
            int nx = cursor.position.x - 1;
            if (!(nx < 0)) cursor.position.x = nx;
        }
        if (is_key_pressed(KEY_DOWN)) {
            cursor.draw = true;
            cursor.blink.timer = 0;
            int ny = cursor.position.y + 1;
            if (!(ny*cursor.height > WINDOW_HEIGHT)) cursor.position.y = ny;
        }
        if (is_key_pressed(KEY_UP)) {
            cursor.draw = true;
            cursor.blink.timer = 0;
            int ny = cursor.position.y - 1;
            if (!(ny < 0)) cursor.position.y = ny;
        }

        for (int k = KEY_A; k <= KEY_Z; k++) {
            if (is_key_pressed(k)) {
                bool shift = is_key_down(KEY_LEFT_SHIFT) | is_key_down(KEY_RIGHT_SHIFT);

                if (cursor.position.x < MAX_COLS && cursor.position.y < MAX_COLS) {
                    text[cursor.position.y][cursor.position.x] = shift ? k : k+32;

                    cursor.draw = true;
                    cursor.blink.timer = 0;
                    int nx = cursor.position.x + 1;
                    if (!(nx*cursor.width > WINDOW_WIDTH)) cursor.position.x = nx;
                }
            }
        }

        if (is_key_pressed(KEY_SPACE)) {
            if (cursor.position.x < MAX_COLS && cursor.position.y < MAX_COLS) {
                text[cursor.position.y][cursor.position.x] = KEY_SPACE;
                cursor.draw = true;
                cursor.blink.timer = 0;
                int nx = cursor.position.x + 1;
                if (!(nx*cursor.width > WINDOW_WIDTH)) cursor.position.x = nx;
            }
        }

        float dt = get_frame_time();
        cursor.blink.timer += dt;
        if (cursor.blink.timer > cursor.blink.threshold) {
            cursor.draw = !cursor.draw;
            cursor.blink.timer = 0;
        }

        begin_drawing(BLACK);
            if (cursor.draw) draw_rectangle(cursor.position.x*cursor.width, cursor.position.y*cursor.height, cursor.width, cursor.height, WHITE);
            for (size_t y = 0; y < MAX_COLS; y++) {
                if (y*cursor.height > WINDOW_HEIGHT) continue;
                for (size_t x = 0; x < MAX_ROWS; x++) {
                    if (x*cursor.width > WINDOW_WIDTH) continue;
                    char *c = &text[y][x];
                    draw_text_at_base(c, 1, cursor.width*x, cursor.height*y+30, WHITE);
                }
            }
        end_drawing();
    }
    close_window();
    return 0;
}
