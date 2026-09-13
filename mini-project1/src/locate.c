#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../include/locate.h"

void execute_locate(token tokens[], int count) {
    if (count < 2) {
        printf("locate: invalid syntax\n");
        return;
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char *path_env = getenv("PATH");

    for (int i = 1; i < count; i++) {
        char *filename = tokens[i].values;
        int found = 0;
        char full_path[PATH_MAX];

        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, filename);
        if (access(full_path, X_OK) == 0) {
            printf("%s\n", full_path);
            found = 1;
        }

        if (path_env) {
            char *path_copy = strdup(path_env);
            char *dir = strtok(path_copy, ":");
            while (dir != NULL) {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);
                if (access(full_path, X_OK) == 0) {
                    printf("%s\n", full_path);
                    found = 1;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }

        if (!found) {
            printf("locate: command not found (%s)\n", filename);
        }
    }
}