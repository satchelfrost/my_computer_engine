#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD "build/"
#define LINUX BUILD "linux/"
#define WINDOWS BUILD "windows/"
#define SRC "src/"
#define ENGINE SRC "engine/"
#define EXTERNAL SRC "external/"
#define SHADERS "shaders/"

const char *my_computer_srcs[] = {
    SRC"my_computer.h",
    ENGINE"my_computer.c",
    ENGINE"obj_loader.c",
    ENGINE"gltf_loader.c",
    ENGINE"geometry.c",
};

const char *shaders[] = {
    "standard_triangle_render.vert.glsl",
    "standard_triangle_render.frag.glsl",
    "standard_text.vert.glsl",
    "standard_text.frag.glsl",
    "primitive_2D.frag.glsl",
    "primitive_2D.vert.glsl",
    "text.vert.glsl",
    "text.frag.glsl",
};

const char *examples[] = {
    "example_model",
    "example_text",
    "example_gltf",
    "example_gltf_animation",
    "example_primitive_2D",
};

struct {
    const char *name;
    const char *impl_define;
    const char *api_define;
    const char *include_dir;
} hdr_modules[] = {
    {
        .name = "stb_image",
        .impl_define = "-DSTB_IMAGE_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
    {
        .name = "stb_truetype",
        .impl_define = "-DSTB_TRUETYPE_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
    {
        .name = "cgltf",
        .impl_define = "-DCGLTF_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
    {
        .name = "raymath",
        .impl_define = "-DRAYMATH_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
    {
        .name = "tinyobj_loader_c",
        .impl_define = "-DTINYOBJ_LOADER_C_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
    {
        .name = "rvk",
        .impl_define = "-DRVK_IMPLEMENTATION",
        .include_dir = EXTERNAL,
    },
// TODO: not using these yet, but I will once I add in the audio library from creese_2D
//    {
//        .name = "miniaudio",
//        .impl_define = "-DMINIAUDIO_IMPLEMENTATION",
//        .include_dir = EXTERNAL,
//    },
//    {
//        .name = "dr_wav",
//        .impl_define = "-DDR_WAV_IMPLEMENTATION",
//        .include_dir = EXTERNAL,
//    },
    {
        .name = "nob",
        .impl_define = "-DNOB_IMPLEMENTATION",
        .include_dir = "./",
    },
};

bool build_header_only_libraries(Cmd *cmd, const char *target, bool force)
{
    for (size_t i = 0; i < ARRAY_LEN(hdr_modules); i++) {
        const char *hdr = temp_sprintf("%s%s.h", hdr_modules[i].include_dir, hdr_modules[i].name);
        const char *obj = temp_sprintf("%s%s.o", (target == "linux") ? LINUX : WINDOWS, hdr_modules[i].name);
        int res = needs_rebuild1(obj, hdr);
        if (res < 0) {
            nob_log(ERROR, "needs rebuild failed: header %s, obj %s\n", hdr, obj);
            return false;
        } else if (!res && !force) {
            continue; // no rebuild necessary
        } else {
            /* build compiler command */
            cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc");
            cmd_append(cmd, hdr_modules[i].impl_define);
            if (hdr_modules[i].api_define) cmd_append(cmd, hdr_modules[i].api_define);
            cmd_append(cmd, "-x", "c", hdr);
            cmd_append(cmd, "-c", "-o", obj);
            if (!cmd_run(cmd)) return false;
        }
    }
    return true;
}

bool compile_shaders(Cmd *cmd)
{
    for (size_t i = 0; i < ARRAY_LEN(shaders); i++) {
        const char *src = temp_sprintf(SHADERS"%s", shaders[i]);
        const char *dst = temp_sprintf(SHADERS"%s.spv", shaders[i]);

        if (needs_rebuild(dst, &src, 1)) {
            char *stage = NULL;
            if (strstr(shaders[i], "frag")) stage = "-fshader-stage=frag";
            if (strstr(shaders[i], "vert")) stage = "-fshader-stage=vert";
            if (strstr(shaders[i], "comp")) stage = "-fshader-stage=comp";
            assert(stage && "shader stage unrecognize");
            cmd_append(cmd, "glslc", stage, "-o", dst, src);
            if (!cmd_run(cmd)) return false;
        }
    }

    return true;
}


bool build_glfw(Cmd *cmd, const char *target)
{
    assert(target);
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;
    const char *obj = temp_sprintf("%srglfw.o", build_dir);
    if (file_exists(obj)) return true;

    cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
    cmd_append(cmd, "-I./"EXTERNAL"glfw/");
    cmd_append(cmd, "-c", EXTERNAL"rglfw.c");
    cmd_append(cmd, "-o", obj);
    cmd_append(cmd, "-lm");
    return cmd_run(cmd);
}

bool build_my_computer(Cmd *cmd, bool force, const char *target)
{
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;
    const char *o = temp_sprintf("%smy_computer.o", build_dir);
    const char *c = temp_sprintf(ENGINE"my_computer.c");

    int my_computer_touched = needs_rebuild(o, my_computer_srcs, ARRAY_LEN(my_computer_srcs));
    if (my_computer_touched < 0) {
        nob_log(ERROR, "needs rebuild failed for my_computer");
        return false;
    }
    if (!my_computer_touched && !force) return true;

    cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
    if (target == "linux") cmd_append(cmd, "-DVULKAN_VALIDATION_ON"); // only use validation if not windows
    cmd_append(cmd, "-I./"EXTERNAL);
    cmd_append(cmd, "-I./"EXTERNAL"glfw/include");
    cmd_append(cmd, "-c", c);
    cmd_append(cmd, "-o", o);
    cmd_append(cmd, "-lm");
    if (!cmd_run(cmd)) return false;

    return true;
}

bool build_example(Cmd *cmd, bool force, const char *target, const char *example_name)
{
    const char *build_dir = (target == "linux") ? LINUX : WINDOWS;

    const char *c = temp_sprintf(SRC"%s.c", example_name);
    const char *e = temp_sprintf("%s%s%s", build_dir, example_name, (target == "windows") ? ".exe" : "");
    const char *glfw = temp_sprintf("%srglfw.o", build_dir);
    const char *my_computer = temp_sprintf("%smy_computer.o", build_dir);

    int src_touched = needs_rebuild1(e, c);
    int my_computer_touched = needs_rebuild1(e, my_computer);
    int glfw_touched = needs_rebuild1(e, glfw);
    if (src_touched < 0)         nob_log(ERROR, "needs rebuild command failed for source %s", example_name);
    if (my_computer_touched < 0) nob_log(ERROR, "needs rebuild command failed for my_computer");
    if (glfw_touched < 0)        nob_log(ERROR, "needs rebuild command failed for example glfw");
    if (!src_touched && !my_computer_touched && !glfw_touched && !force) return true;

    cmd_append(cmd, (target == "linux") ? "gcc" : "x86_64-w64-mingw32-gcc", "-Wall", "-Wextra", "-g");
    cmd_append(cmd, "-I./"EXTERNAL);
    cmd_append(cmd, "-I./"EXTERNAL"glfw/include");
    cmd_append(cmd, "-I./"SRC);
    cmd_append(cmd, "-o", e);
    cmd_append(cmd, c, glfw, my_computer);
    for (size_t i = 0; i < ARRAY_LEN(hdr_modules); i++) {
        const char *obj = temp_sprintf("%s%s.o", (target == "linux") ? LINUX : WINDOWS, hdr_modules[i].name);
        cmd_append(cmd, obj);
    }
    if (target == "linux") cmd_append(cmd, "-lm", "-lvulkan");
    else                   cmd_append(cmd, "-L./"EXTERNAL, "-l:vulkan-1.lib", "-lgdi32");
    return cmd_run(cmd);
}

void log_usage(const char *program)
{
    printf("usage: %s [options]\n", program);
    printf("    --help\n");
    printf("    --clean, force clean build\n");
    printf("    --list, lists example numbers/names\n");
    printf("    --target, build target (e.g. windows and linux)\n");
    printf("    --run <example number> <args>, run after building (only for linux)\n");
    printf("    --renderdoc <example number> <args>, (only for linux) expects renderdoc/renderdoccmd in path (https://renderdoc.org/builds)\n");
    printf("    --debug <example number> <args>, (only for linux) expects gf2 in path (https://github.com/nakst/gf)\n");
}

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Args;

struct {
    const char *program;
    const char *target;
    int example_number;
    bool clean;
    bool renderdoc;
    bool debug;
    bool run;
    bool help;
    bool list;
    Args args;
} config;

bool parse_cmd_args(int argc, char **argv)
{
    config.program = shift(argv, argc);

    while (argc) {
        const char *flag = shift(argv, argc);
        if (!strcmp("--help", flag)) {
            config.help = true;
        } else if (!strcmp("--clean", flag)) {
            config.clean = true;
            nob_log(INFO, "executing clean build");
        } else if (!strcmp("--list", flag)) {
            config.list = true;
        } else if (!strcmp("--renderdoc", flag)) {
            config.renderdoc = true;
            if (argc <= 0) {
                nob_log(ERROR, "usage: `./nob --renderdoc <executable> <args>`");
                return false;
            }
            config.example_number = atoi(shift(argv, argc));
            if (config.example_number < 0 || config.example_number >= ARRAY_LEN(examples)) {
                nob_log(ERROR, "example number %d was out of range [0, %zu]", config.example_number, ARRAY_LEN(examples)-1);
                nob_log(ERROR, "usage: `./nob --renderdoc <example number> <args>`");
                return false;
            }
            while (argc) {
                const char *arg = shift(argv, argc);
                da_append(&config.args, arg);
            }
        } else if (!strcmp("--run", flag)) {
            config.run = true;
            if (argc <= 0) {
                nob_log(ERROR, "--run usage: `./nob --run <example number> <args>`");
                return false;
            }
            config.example_number = atoi(shift(argv, argc));
            if (config.example_number < 0 || config.example_number >= ARRAY_LEN(examples)) {
                nob_log(ERROR, "example number %d was out of range [0, %zu]", config.example_number, ARRAY_LEN(examples)-1);
                nob_log(ERROR, "--run usage: `./nob --run <example number> <args>`");
                return false;
            }
            while (argc) {
                const char *arg = shift(argv, argc);
                da_append(&config.args, arg);
            }
        } else if (!strcmp("--debug", flag)) {
            config.debug = true;
            if (argc <= 0) {
                nob_log(ERROR, "--debug usage: `./nob --debug <example number> <args>`");
                return false;
            }
            config.example_number = atoi(shift(argv, argc));
            if (config.example_number < 0 || config.example_number >= ARRAY_LEN(examples)) {
                nob_log(ERROR, "example number %d was out of range [0, %zu]", config.example_number, ARRAY_LEN(examples)-1);
                nob_log(ERROR, "--debug usage: `./nob --debug <example number> <args>`");
                return false;
            }
            while (argc) {
                const char *arg = shift(argv, argc);
                da_append(&config.args, arg);
            }
        } else if (!strcmp("--target", flag)) {
            const char *target = shift(argv, argc);
            if (!strcmp(target, "windows")) config.target = "windows";
            if (!strcmp(target, "linux"))   config.target = "linux";
        } else {
            nob_log(ERROR, "unrecognized flag `%s`", flag);
            log_usage(config.program);
            return false;
        }
    }

    /* default target linux */
    if (!config.target) config.target = "linux";

    if (config.run && config.renderdoc) {
        nob_log(ERROR, "--run and --renderdoc not compatible together (just choose one or the other)");
        return false;
    }

    return true;
}

bool launch_renderdoc(Cmd *cmd)
{
    /* first run the renderdoc cmd to capture the snapshot */
    cmd_append(cmd, "renderdoccmd", "capture", "-d", ".", "-c", "snapshot", "-w");
    cmd_append(cmd, temp_sprintf("./"LINUX"%s", examples[config.example_number]));
    for (size_t i = 0; i < config.args.count; i++)
        cmd_append(cmd, config.args.items[i]);
    if (!cmd_run(cmd)) return false;

    /* next read the snapshot and launch renderdoc */
    File_Paths paths = {0};
    read_entire_dir(".", &paths);
    for (size_t i = 0; i < paths.count; i++) {
        const char *file_name = paths.items[i];
        /* open the first recognized capture file */
        if (strstr(file_name, ".rdc")) {
            nob_log(NOB_INFO, "reading renderdoc file %s", file_name);
            cmd_append(cmd, "renderdoc", file_name);
            if (!cmd_run_sync_and_reset(cmd)) return false;

            /* cleanup capture file */
            nob_log(NOB_INFO, "removing renderdoc file %s", file_name);
            cmd_append(cmd, "rm", file_name);
            if (!cmd_run_sync_and_reset(cmd)) return false;
            break;
        }
    }

    return true;
}

bool launch_exec(Cmd *cmd)
{
    cmd_append(cmd, temp_sprintf("./"LINUX"%s", examples[config.example_number]));
    for (size_t i = 0; i < config.args.count; i++)
        cmd_append(cmd, config.args.items[i]);
    if (!cmd_run(cmd)) return false;

    return true;
}

bool launch_gf2(Cmd *cmd)
{
    cmd_append(cmd, "gf2");
    cmd_append(cmd, "-ex", "start");
    if (config.args.count) cmd_append(cmd, "--args");
    cmd_append(cmd, temp_sprintf("./"LINUX"%s", examples[config.example_number]));
    for (size_t i = 0; i < config.args.count; i++)
        cmd_append(cmd, config.args.items[i]);
    return cmd_run(cmd);
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!parse_cmd_args(argc, argv))   return 1;
    if (config.help) {
        log_usage(config.program);
        return 0;
    }
    if (config.list) {
        for (size_t i = 0; i < ARRAY_LEN(examples); i++)
            printf("%zu - %s\n", i, examples[i]);
        return 0;
    }

    if (!mkdir_if_not_exists(BUILD))   return 1;
    if (!mkdir_if_not_exists(LINUX))   return 1;
    if (!mkdir_if_not_exists(WINDOWS)) return 1;

    Cmd cmd = {0};

    if (!build_header_only_libraries(&cmd, config.target, config.clean)) return 1;
    if (!build_glfw(&cmd, config.target)) return 1;
    if (!build_my_computer(&cmd, config.clean, config.target)) return 1;
    for (size_t i = 0; i < ARRAY_LEN(examples); i++)
        if (!build_example(&cmd, config.clean, config.target, examples[i])) return 1;
    if (!compile_shaders(&cmd)) return 1;

    if (config.run)       if (!launch_exec(&cmd))      return 1;
    if (config.renderdoc) if (!launch_renderdoc(&cmd)) return 1;
    if (config.debug) {
        if (!launch_gf2(&cmd)) {
            printf("To install gf2 clone https://github.com/nakst/gf\n");
            return 1;
        }
    }

    return 0;
}
