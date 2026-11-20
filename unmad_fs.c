#include "unmad_fs_core.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    if (fs_init(DISK_FILE) != 0) {
        perror("Failed to open disk.img");
        return 1;
    }

    char line[8192];
    char prompt_path[256];
    char out_buf[16384];

    while (1) {
        fs_get_cwd_path(prompt_path, sizeof(prompt_path));
        printf("unmad-fs:%s> ", prompt_path);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }

        out_buf[0] = '\0';
        fs_execute_command(line, out_buf, sizeof(out_buf));
        if (strcmp(out_buf, "__CLEAR_SCREEN__\n") == 0) {
            printf("\033[H\033[J");
        } else if (out_buf[0]) {
            printf("%s", out_buf);
        }
    }

    fs_close();
    return 0;
}
