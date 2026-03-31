#define NOB_IMPLEMENTATION
#include "lib/nob.h"

#include <string.h>

#define SRC_FOLDER "src/"
#define LIB_FOLDER "lib/"
#define BUILD_FOLDER "build/"
#define INCLUDE_FOLDER "include/"

typedef enum BuildType {
  BUILD_TYPE_DEBUG,
  BUILD_TYPE_RELEASE,
} BuildType;

int build_gen(void);
int build_main(BuildType type);

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  if (argc == 1) {
    build_main(BUILD_TYPE_RELEASE);
  } else if (strcmp("gen", argv[1]) == 0) {
    build_gen();
  } else if (strcmp("main", argv[1]) == 0) {
    if (argc == 2 && strcmp("debug", argv[2]) == 0) {
      build_main(BUILD_TYPE_DEBUG);
    } else {
      build_main(BUILD_TYPE_RELEASE);
    }
  }

  return 0;
}

int build_main(BuildType type) {
  Nob_Cmd cmd = {0};

  nob_cc(&cmd);
  nob_cc_flags(&cmd);

  nob_cmd_append(
      &cmd,
      "-std=c99",
      "-D_DEFAULT_SOURCE"); // https://github.com/tsoding/nob.h/issues/152

  if (type == BUILD_TYPE_DEBUG) {
    nob_cmd_append(
        &cmd,
        "-pg", // enable gprof profiling
        "-ggdb");
  } else {
    nob_cmd_append(&cmd, "-O2");
  }

  nob_cc_output(&cmd, BUILD_FOLDER "main");

  nob_cc_inputs(
      &cmd,
      SRC_FOLDER "main.c",
      LIB_FOLDER "glad/src/glad.c",
      LIB_FOLDER "tinyfiledialogs/tinyfiledialogs.c");

  nob_cmd_append(&cmd, "-I" INCLUDE_FOLDER);

  nob_cmd_append(&cmd, "-I" LIB_FOLDER);
  nob_cmd_append(&cmd, "-I" LIB_FOLDER "glad/include");
  nob_cmd_append(&cmd, "-I" LIB_FOLDER "tinyfiledialogs");

  nob_cmd_append(&cmd, "-I" LIB_FOLDER "glfw/include");
  nob_cmd_append(&cmd, "-L" LIB_FOLDER "glfw/build/src");

#if _WIN32
  nob_cmd_append(
      &cmd,
      "-lm",
      "-lpthread",
      "-lglfw3",
      "-lgdi32",
      // tinyfiledialogs links with -lcomdlg32 -lole32 on windows
      "-lcomdlg32",
      "-lole32");
#else
  nob_cmd_append(
      &cmd,
      "-lm",
      "-ldl",
      "-lpthread",
      "-lglfw3");
#endif

  if (!nob_cmd_run(&cmd)) {
    return 1;
  }

  return 0;
}

int build_gen(void) {
  Nob_Cmd cmd = {0};

  nob_cc(&cmd);
  nob_cc_flags(&cmd);

  nob_cmd_append(
      &cmd,
      "-std=c99",
      "-D_DEFAULT_SOURCE");

  nob_cc_output(&cmd, BUILD_FOLDER "gen");

  nob_cc_inputs(&cmd, SRC_FOLDER "gen.c");
  nob_cmd_append(&cmd, "-I" LIB_FOLDER);

  if (!nob_cmd_run(&cmd)) {
    return 1;
  }

  return 1;
}
