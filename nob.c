/*
    This file is the main (and only) build file for sfte.
    It's responsible for building the amalgamation (sfte.h) file,
    installing the binary and, obviously, compiling the project.

    TODO:
    Add support for other/custom backends.
*/

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

typedef enum {
    MODE_INSTALL,
    MODE_BUILD,
    MODE_DEV,
} compile_mode;

static uint8_t install_target(void) {
    const char *prefix = getenv("PREFIX");
    Nob_String_Builder bin = {0};
    Nob_String_Builder desktop = {0};
    Nob_String_Builder icon = {0};

    if (prefix) {
        nob_sb_append_cstr(&bin, prefix);
        nob_sb_append_cstr(&bin, "/bin");
        nob_sb_append_cstr(&desktop, prefix);
        nob_sb_append_cstr(&desktop, "/share/applications");
        nob_sb_append_cstr(&icon, prefix);
        nob_sb_append_cstr(&icon, "/share/icons/hicolor/scalable/apps");
    } else {
        if (geteuid() == 0) {  // sudo
            nob_sb_append_cstr(&bin, "/usr/local/bin");
            nob_sb_append_cstr(&desktop, "/usr/local/share/applications");
            nob_sb_append_cstr(&icon, "/usr/local/share/icons/hicolor/scalable/apps");
        } else {
            const char *home = getenv("HOME");
            if (!home) {
                nob_log(NOB_ERROR, "HOME environment variable not set.");
                return 1;
            }
            nob_sb_append_cstr(&bin, home);
            nob_sb_append_cstr(&bin, "/.local/bin");
            nob_sb_append_cstr(&desktop, home);
            nob_sb_append_cstr(&desktop, "/.local/share/applications");
            nob_sb_append_cstr(&icon, home);
            nob_sb_append_cstr(&icon, "/.local/share/icons/hicolor/scalable/apps");
        }
    }

    nob_sb_append_null(&bin);
    nob_sb_append_null(&desktop);
    nob_sb_append_null(&icon);

    if (!nob_mkdir_if_not_exists(bin.items)) return 1;
    if (!nob_mkdir_if_not_exists(desktop.items)) return 1;
    if (!nob_mkdir_if_not_exists(icon.items)) return 1;

    const char *bin_path = nob_temp_sprintf("%s/sfte", bin.items);
    remove(bin_path);  // Unlink the binary before replacing
    if (!nob_copy_file("sfte", bin_path)) return 1;

    const char *desktop_content = "[Desktop Entry]\n"
                                  "Type=Application\n"
                                  "Name=sfte\n"
                                  "GenericName=Terminal Emulator\n"
                                  "Comment=single-file terminal emulator\n"
                                  "Exec=sfte\n"
                                  "Icon=sfte\n"
                                  "Terminal=false\n"
                                  "Categories=System;TerminalEmulator;Utility;\n"
                                  "StartupNotify=true\n";
    const char *desktop_path = nob_temp_sprintf("%s/sfte.desktop", desktop.items);
    if (!nob_write_entire_file(desktop_path, desktop_content, strlen(desktop_content))) return 1;

    const char *icon_content = "<?xml version='1.0' encoding='UTF-8' ?>\n"
                               "<svg version='1.1' width='32' height='32' "
                               "xmlns='http://www.w3.org/2000/svg' shape-rendering='crispEdges'>\n"
                               "<rect x='1' y='2' width='30' height='1' fill='#FFFFFF' />\n"
                               "<rect x='0' y='3' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='1' y='3' width='1' height='1' fill='#595652' />\n"
                               "<rect x='2' y='3' width='28' height='1' fill='#000000' />\n"
                               "<rect x='30' y='3' width='1' height='1' fill='#595652' />\n"
                               "<rect x='31' y='3' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='0' y='4' width='1' height='24' fill='#9BADB7' />\n"
                               "<rect x='1' y='4' width='2' height='1' fill='#000000' />\n"
                               "<rect x='3' y='4' width='26' height='2' fill='#222034' />\n"
                               "<rect x='29' y='4' width='2' height='1' fill='#000000' />\n"
                               "<rect x='31' y='4' width='1' height='24' fill='#9BADB7' />\n"
                               "<rect x='1' y='5' width='1' height='23' fill='#000000' />\n"
                               "<rect x='2' y='5' width='1' height='21' fill='#222034' />\n"
                               "<rect x='29' y='5' width='1' height='21' fill='#222034' />\n"
                               "<rect x='30' y='5' width='1' height='23' fill='#000000' />\n"
                               "<rect x='3' y='6' width='3' height='1' fill='#222034' />\n"
                               "<rect x='6' y='6' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='7' y='6' width='22' height='1' fill='#222034' />\n"
                               "<rect x='3' y='7' width='2' height='1' fill='#222034' />\n"
                               "<rect x='5' y='7' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='6' y='7' width='3' height='1' fill='#FFFFFF' />\n"
                               "<rect x='9' y='7' width='4' height='1' fill='#222034' />\n"
                               "<rect x='13' y='7' width='1' height='7' fill='#FFFFFF' />\n"
                               "<rect x='14' y='7' width='15' height='6' fill='#222034' />\n"
                               "<rect x='3' y='8' width='1' height='19' fill='#222034' />\n"
                               "<rect x='4' y='8' width='1' height='2' fill='#FFFFFF' />\n"
                               "<rect x='5' y='8' width='1' height='1' fill='#000000' />\n"
                               "<rect x='6' y='8' width='1' height='2' fill='#9BADB7' />\n"
                               "<rect x='7' y='8' width='2' height='1' fill='#000000' />\n"
                               "<rect x='9' y='8' width='3' height='1' fill='#222034' />\n"
                               "<rect x='12' y='8' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='5' y='9' width='1' height='1' fill='#222034' />\n"
                               "<rect x='7' y='9' width='4' height='1' fill='#222034' />\n"
                               "<rect x='11' y='9' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='12' y='9' width='1' height='1' fill='#000000' />\n"
                               "<rect x='4' y='10' width='1' height='1' fill='#000000' />\n"
                               "<rect x='5' y='10' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='6' y='10' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='7' y='10' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='8' y='10' width='3' height='1' fill='#222034' />\n"
                               "<rect x='11' y='10' width='1' height='1' fill='#000000' />\n"
                               "<rect x='12' y='10' width='1' height='3' fill='#222034' />\n"
                               "<rect x='4' y='11' width='1' height='2' fill='#222034' />\n"
                               "<rect x='5' y='11' width='1' height='1' fill='#000000' />\n"
                               "<rect x='6' y='11' width='1' height='2' fill='#9BADB7' />\n"
                               "<rect x='7' y='11' width='1' height='1' fill='#000000' />\n"
                               "<rect x='8' y='11' width='1' height='2' fill='#FFFFFF' />\n"
                               "<rect x='9' y='11' width='3' height='2' fill='#222034' />\n"
                               "<rect x='5' y='12' width='1' height='1' fill='#222034' />\n"
                               "<rect x='7' y='12' width='1' height='1' fill='#222034' />\n"
                               "<rect x='4' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='5' y='13' width='2' height='1' fill='#FFFFFF' />\n"
                               "<rect x='7' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='8' y='13' width='1' height='1' fill='#000000' />\n"
                               "<rect x='9' y='13' width='2' height='14' fill='#222034' />\n"
                               "<rect x='11' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='12' y='13' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='14' y='13' width='1' height='1' fill='#FFFFFF' />\n"
                               "<rect x='15' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='16' y='13' width='2' height='14' fill='#222034' />\n"
                               "<rect x='18' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='19' y='13' width='3' height='1' fill='#FFFFFF' />\n"
                               "<rect x='22' y='13' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='23' y='13' width='6' height='14' fill='#222034' />\n"
                               "<rect x='4' y='14' width='2' height='1' fill='#000000' />\n"
                               "<rect x='6' y='14' width='1' height='1' fill='#9BADB7' />\n"
                               "<rect x='7' y='14' width='1' height='1' fill='#000000' />\n"
                               "<rect x='8' y='14' width='1' height='13' fill='#222034' />\n"
                               "<rect x='11' y='14' width='5' height='1' fill='#000000' />\n"
                               "<rect x='18' y='14' width='5' height='1' fill='#000000' />\n"
                               "<rect x='4' y='15' width='2' height='12' fill='#222034' />\n"
                               "<rect x='6' y='15' width='1' height='1' fill='#000000' />\n"
                               "<rect x='7' y='15' width='1' height='12' fill='#222034' />\n"
                               "<rect x='11' y='15' width='5' height='12' fill='#222034' />\n"
                               "<rect x='18' y='15' width='5' height='12' fill='#222034' />\n"
                               "<rect x='6' y='16' width='1' height='11' fill='#222034' />\n"
                               "<rect x='2' y='26' width='1' height='3' fill='#000000' />\n"
                               "<rect x='29' y='26' width='1' height='3' fill='#000000' />\n"
                               "<rect x='3' y='27' width='26' height='2' fill='#000000' />\n"
                               "<rect x='0' y='28' width='1' height='1' fill='#847E87' />\n"
                               "<rect x='1' y='28' width='1' height='1' fill='#696A6A' />\n"
                               "<rect x='30' y='28' width='1' height='1' fill='#696A6A' />\n"
                               "<rect x='31' y='28' width='1' height='1' fill='#847E87' />\n"
                               "<rect x='1' y='29' width='30' height='1' fill='#847E87' />\n"
                               "</svg>\n";
    const char *icon_path = nob_temp_sprintf("%s/sfte.svg", icon.items);
    if (!nob_write_entire_file(icon_path, icon_content, strlen(icon_content))) return 1;

    nob_log(NOB_INFO, "Installation complete.");
    if (geteuid() != 0 && !prefix) nob_log(NOB_WARNING, "Ensure %s is in your $PATH.", bin.items);

    return 0;
}

static uint8_t build_amalgamation(const char *input_path, const char *output_path) {
    Nob_String_Builder out = {0};

    nob_sb_append_cstr(&out, "/*\n");
    nob_sb_append_cstr(&out, " * AMALGAMATED RELEASE BUILD - EDIT sfte_dev.h FOR REPO CHANGES\n");
    nob_sb_append_cstr(&out, " */\n\n");

    Nob_String_Builder sfte = {0};
    if (!nob_read_entire_file(input_path, &sfte)) return false;

    Nob_String_View sfte_sv = nob_sv_from_parts(sfte.items, sfte.count);

    while (sfte_sv.count > 0) {
        Nob_String_View line = nob_sv_chop_by_delim(&sfte_sv, '\n');
        Nob_String_View trimmed = nob_sv_trim(line);

        if (nob_sv_eq(trimmed, nob_sv_from_cstr("#include \"vendor/xdg-shell.h\""))) {
            nob_sb_append_cstr(
                &out,
                "/*=== xdg-shell.h =========================================================*/\n");
            if (!nob_read_entire_file("vendor/xdg-shell.h", &out)) return false;
        } else if (nob_sv_eq(trimmed, nob_sv_from_cstr("#include \"vendor/xdg-shell.c\""))) {
            nob_sb_append_cstr(
                &out,
                "/*=== xdg-shell.c =========================================================*/\n");
            if (!nob_read_entire_file("vendor/xdg-shell.c", &out)) return false;
        } else if (nob_sv_eq(trimmed, nob_sv_from_cstr("#include \"vendor/stb_truetype.h\""))) {
            nob_sb_append_cstr(
                &out,
                "/*=== stb_truetype.h ======================================================*/\n");
            if (!nob_read_entire_file("vendor/stb_truetype.h", &out)) return false;
        } else if (nob_sv_eq(trimmed, nob_sv_from_cstr("#include \"vendor/stb_image.h\""))) {
            nob_sb_append_cstr(
                &out,
                "/*=== stb_image.h =========================================================*/\n");
            if (!nob_read_entire_file("vendor/stb_image.h", &out)) return false;
        } else if (nob_sv_eq(trimmed, nob_sv_from_cstr("#include \"vendor/re.h\""))) {
            nob_sb_append_cstr(
                &out,
                "/*=== tiny-regex-c ========================================================*/\n");
            if (!nob_read_entire_file("vendor/re.h", &out)) return false;
        } else {
            nob_sb_append_buf(&out, line.data, line.count);
            nob_sb_append_cstr(&out, "\n");
        }
    }

    nob_log(NOB_INFO, "generating amalgamation to %s", output_path);
    uint8_t ok = nob_write_entire_file(output_path, out.items, out.count);

    nob_sb_free(out);
    nob_sb_free(sfte);

    return ok;
}

static void print_usage(const char *program) {
    nob_log(NOB_INFO,
            "usage: %s <mode>\n"
            "\t- mode=install: compiles from 'sfte.h' and installs the app on the system.\n"
            "\t- mode=build:   compiles from 'sfte.h' without installation.\n"
            "\t- mode=dev:     compiles from 'sfte_dev.h' and builds the amalgamation file.\n",
            program);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);
    if (argc != 1) {
        print_usage(program);
        return 0;
    }

    const char *mode_str = nob_shift_args(&argc, &argv);
    compile_mode mode;

    if (!strcmp(mode_str, "install"))
        mode = MODE_INSTALL;
    else if (!strcmp(mode_str, "build"))
        mode = MODE_BUILD;
    else if (!strcmp(mode_str, "dev"))
        mode = MODE_DEV;
    else {
        print_usage(program);
        return 0;
    }

    Nob_Cmd cmd = {0};

    if (mode == MODE_DEV) {
        nob_cmd_append(&cmd, "wayland-scanner", "client-header", "vendor/xdg-shell.xml",
                       "vendor/xdg-shell.h");
        if (!nob_cmd_run_sync(cmd)) return 1;
        cmd.count = 0;

        nob_cmd_append(&cmd, "wayland-scanner", "private-code", "vendor/xdg-shell.xml",
                       "vendor/xdg-shell.c");
        if (!nob_cmd_run_sync(cmd)) return 1;
        cmd.count = 0;

        if (!build_amalgamation("sfte_dev.h", "sfte.h")) return 1;
    }

    if (nob_file_exists("config.c") <= 0) {
        nob_log(NOB_INFO, "config.c not found, generating from config.def.c");
        if (!nob_copy_file("config.def.c", "config.c")) return 1;
    }

    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-O3", "-flto=auto", "-march=native", "config.c",
                   "-o", "sfte", "-lwayland-client", "-lrt", "-lm", "-D_GNU_SOURCE", "-lutil",
                   "-lxkbcommon", "-std=c11");

    if (mode == MODE_DEV) nob_cmd_append(&cmd, "-DSFTE_DEV_ENV");

    if (!nob_cmd_run_sync(cmd)) return 1;

    if (mode == MODE_INSTALL && install_target()) return 1;

    return 0;
}
