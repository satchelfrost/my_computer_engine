# My Computer Engine

**A basic graphics library heavily inspired by raylib, but specialized for OpenXR and Vulkan.**

The primary goal is to create the `my_computer` application for the meta quest.

## features

* Written in plain C code (C99) using snake_case (except for external libraries like raymath)
* Raylib inspired API (e.g. init_window)
* Vulkan based hardware acceleration
* Desktop renderer (non-VR)
* AR/VR renderer for meta quest (i.e. OpenXR - WORK IN PROGRESS)

## limitations
* Feature parity with raylib's API is not a design goal
* linux development is the only thing currently supported
* AR/VR is limited to the meta quest
* Currently, there are no immediate plans for desktop VR linux

## basic example (non-VR)

```c
#include "my_computer.h"

int main()
{
    init_window(800, 600, "my_computer_engine - example text");

    const char *text = "Hello World!";
    size_t text_len = strlen(text);
    int text_width = measure_text(text, text_len);

    while (!window_should_close()) {
        begin_drawing(WHITE);
        draw_text_at_base(text, strlen(text), 800/2 - text_width/2, 600/2, BLACK);
        end_drawing();
    }

    close_window();

    return 0;
}
```

## linux desktop dependencies not included in this repo

* glslc - shader compiler
* vulkan validation layers
* x86_64-w64-mingw32-gcc (not strictly required)

## android dependencies for meta quest build
* Android NDK version: `21.0.6113669`
* Android SDK using API: `29` (specifically SDK tools `29.0.2`)
* adb
* keytool

## development tools (not strictly required)
* [nob](https://github.com/tsoding/nob.h) - build system (included)
* [tatr](https://github.com/tsoding/tatr) - task tracking
* [renderdoc](https://renderdoc.org/builds) - computer graphics debugging
* [gf2](https://github.com/nakst/gf) - front end for gdb
* x86_64-w64-mingw32-gcc - cross compiling to windows

## AI Usage
Since this is just a hobby project, where speed of development is not the main
goal, AI usage in this project is limited to search. However, if you were inspired by any ideas from this project,
or perhaps used AI to create a new project which was inspired from this project, I would appreciate some acknowledgement ;).

## Getting started
* TODO

## What is my_computer?
* TODO
