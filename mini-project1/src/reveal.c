#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/reveal.h"

int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void reveal_dir(const char *path, int show_hidden, int recursive, const char *prefix) {
    DIR *dir = opendir(path);
    if (!dir) return;

    char **entries = NULL;
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_hidden && entry->d_name[0] == '.') continue;
        entries = realloc(entries, sizeof(char *) * (count + 1));
        entries[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dir);

    qsort(entries, count, sizeof(char *), compare_strings);

    for (int i = 0; i < count; i++) {
        char subpath[2048];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entries[i]);
        struct stat st;
        int is_directory = (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode));
        int is_dot = (strcmp(entries[i], ".") == 0 || strcmp(entries[i], "..") == 0);

        if (is_directory && !is_dot && recursive) {
            printf("%s%s/\n", prefix, entries[i]);
            char new_prefix[2048];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, entries[i]);
            reveal_dir(subpath, show_hidden, recursive, new_prefix);
        } else {
            printf("%s%s\n", prefix, entries[i]);
        }
        free(entries[i]);
    }
    free(entries);
}

void execute_reveal(token tokens[], int count, char *home_dir, char *prev_dir) {
    int show_hidden = 0, recursive = 0;
    char target_path[1024] = ".";
    int path_set = 0;

    for (int i = 1; i < count; i++) {
        if (tokens[i].values[0] == '-' && strlen(tokens[i].values) > 1 && strcmp(tokens[i].values, "-") != 0) {
            for (int j = 1; j < (int)strlen(tokens[i].values); j++) {
                if (tokens[i].values[j] == 'a') show_hidden = 1;
                else if (tokens[i].values[j] == 't') recursive = 1;
                else {
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
        } else {
            if (path_set) {
                printf("reveal: invalid syntax\n");
                return;
            }
            path_set = 1;
            if (strcmp(tokens[i].values, "~") == 0) strcpy(target_path, home_dir);
            else if (strcmp(tokens[i].values, "-") == 0) {
                if (strlen(prev_dir) == 0) {
                    printf("reveal: no such directory\n");
                    return;
                }
                strcpy(target_path, prev_dir);
            }
            else strcpy(target_path, tokens[i].values);
        }
    }

    struct stat st;
    if (stat(target_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("reveal: no such directory\n");
        return;
    }
    reveal_dir(target_path, show_hidden, recursive, "");
}