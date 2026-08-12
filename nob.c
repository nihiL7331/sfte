#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "wayland-scanner", "client-header", "xdg-shell.xml", "xdg-shell.h");
    if (!nob_cmd_run_sync(cmd)) return 1;
    cmd.count = 0;

    nob_cmd_append(&cmd, "wayland-scanner", "private-code", "xdg-shell.xml", "xdg-shell.c");
    if (!nob_cmd_run_sync(cmd)) return 1;
    cmd.count = 0;

    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-O3", "config.c", "-o", "sfte", "-lwayland-client", "-lrt", "-lm");
    if (!nob_cmd_run_sync(cmd)) return 1;

    return 0;
}
