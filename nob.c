#define NOB_IMPLEMENTATION
#include "include/nob.h"

#include <string.h>

#define SRC_FOLDER "src/"
#define BUILD_FOLDER "build/"
#define INCLUDE_FOLDER "include/"

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  if (argc == 1) {
    Nob_Cmd cmd = {0};

    // compiler, its flags and the ouput executable
    nob_cmd_append(&cmd, "clang");
    nob_cc_flags(&cmd);
    nob_cmd_append(
        &cmd,
        "-pg", // enable gprof profiling
        "-ggdb",
        "-std=c99",
        "-D_DEFAULT_SOURCE"); // https://github.com/tsoding/nob.h/issues/152
    //"-pedantic");
    nob_cc_output(&cmd, BUILD_FOLDER "main");

    // src files
    nob_cc_inputs(
        &cmd,
        SRC_FOLDER "main.c",
        SRC_FOLDER "glad.c",
        SRC_FOLDER "tinyfiledialogs.c");

    // link flags: include directory and libraries
    nob_cmd_append(&cmd, "-I" INCLUDE_FOLDER);
    nob_cmd_append(
        &cmd,
        "-lm",
        "-ldl",
        "-lpthread",
        "-lglfw");

    if (!nob_cmd_run(&cmd)) {
      return 1;
    }
  } else if (strcmp("gen", argv[1]) == 0) {
    Nob_Cmd cmd = {0};

    // compiler, its flags and the ouput executable
    nob_cmd_append(&cmd, "clang");
    nob_cc_flags(&cmd);
    nob_cmd_append(
        &cmd,
        "-std=c99",
        "-D_DEFAULT_SOURCE"); // https://github.com/tsoding/nob.h/issues/152
    //"-pedantic");
    nob_cc_output(&cmd, BUILD_FOLDER "gen");

    // src files
    nob_cc_inputs(
        &cmd,
        SRC_FOLDER "gen.c");

    // link flags: include directory and libraries
    nob_cmd_append(&cmd, "-I" INCLUDE_FOLDER);

    if (!nob_cmd_run(&cmd)) {
      return 1;
    }
  }

  return 0;
}
