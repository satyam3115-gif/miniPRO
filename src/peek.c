#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../include/peek.h"

#define CHUNK_SIZE 1024
void process_line(char *line, ssize_t len, int number_lines, int *line_num, int is_reverse) {
    int is_empty = 1;
    for (ssize_t i = 0; i < len; i++) {
        if (line[i] != '\n' && line[i] != '\0') {
            is_empty = 0;
            break;
        }
    }
    if (!is_empty && number_lines) {
        if (is_reverse) printf("%d ", (*line_num)--);
        else printf("%d ", (*line_num)++);
    }
    for (ssize_t i = 0; i < len; i++) {
        if (line[i] != '\0') putchar(line[i]);
    }
    putchar('\n');
}

void peek_normal(int fd, int number_lines, int *line_num) {
    char buf[4096];
    ssize_t bytes;
    int at_start = 1;
    
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            if (at_start && buf[i] != '\n') {
                if (number_lines) printf("%d ", (*line_num)++);
                at_start = 0;
            }
            if (buf[i] != '\n') putchar(buf[i]);
            if (buf[i] == '\n') {
                putchar('\n');
                at_start = 1;
            }
        }
    }
}

void peek_reverse_seekable(int fd, int number_lines, int *line_num) {
    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) return;
    int non_empty_count = 0;
    if (number_lines) {
        lseek(fd, 0, SEEK_SET); 
        char count_buf[CHUNK_SIZE];
        ssize_t c_bytes;
        int line_has_content = 0;
        while ((c_bytes = read(fd, count_buf, CHUNK_SIZE)) > 0) {
            for (ssize_t i = 0; i < c_bytes; i++) {
                if (count_buf[i] == '\n') {
                    if (line_has_content) non_empty_count++;
                    line_has_content = 0;
                } else {
                    line_has_content = 1;
                }
            }
        }
        if (line_has_content) non_empty_count++;
        
        lseek(fd, 0, SEEK_END); 
    }

    char buf[CHUNK_SIZE];
    off_t pos = size;
    char *line_buf = NULL;
    size_t line_len = 0;
    int rev_num = number_lines ? (*line_num + non_empty_count - 1) : 1;

    while (pos > 0) {
        ssize_t to_read = (pos >= CHUNK_SIZE) ? CHUNK_SIZE : pos;
        pos -= to_read;
        lseek(fd, pos, SEEK_SET);
        read(fd, buf, to_read);

        for (ssize_t i = to_read - 1; i >= 0; i--) {
            if (buf[i] == '\n' && (pos != size - to_read || i != to_read - 1)) {
                char *rev = malloc(line_len + 1);
                for(size_t j = 0; j < line_len; j++) rev[j] = line_buf[line_len - 1 - j];
                rev[line_len] = '\0';
                process_line(rev, line_len, number_lines, &rev_num, 1);
                free(rev);
                line_len = 0;
                free(line_buf);
                line_buf = NULL;
            } else if (buf[i] != '\n') {
                line_buf = realloc(line_buf, line_len + 1);
                line_buf[line_len++] = buf[i];
            }
        }
    }
    if (line_len > 0) {
        char *rev = malloc(line_len + 1);
        for(size_t j = 0; j < line_len; j++) rev[j] = line_buf[line_len - 1 - j];
        rev[line_len] = '\0';
        process_line(rev, line_len, number_lines, &rev_num, 1);
        free(rev);
        free(line_buf);
    }
    if (number_lines) *line_num += non_empty_count;
}

void peek_reverse_stream(int fd, int number_lines, int *line_num) {
    char *full = NULL;
    size_t total = 0;
    char buf[4096];
    ssize_t bytes;
    
    while((bytes = read(fd, buf, sizeof(buf))) > 0) {
        full = realloc(full, total + bytes);
        memcpy(full + total, buf, bytes);
        total += bytes;
    }
    
    if (total == 0) return;
    int non_empty_count = 0;
    if (number_lines) {
        int line_has_content = 0;
        for(size_t k = 0; k < total; k++) {
            if (full[k] == '\n') {
                if (line_has_content) non_empty_count++;
                line_has_content = 0;
            } else {
                line_has_content = 1;
            }
        }
        if (line_has_content) non_empty_count++;
    }
    char *line_buf = NULL;
    size_t line_len = 0;
    int rev_num = number_lines ? (*line_num + non_empty_count - 1) : 1;

    for(ssize_t i = total - 1; i >= 0; i--) {
        if (full[i] == '\n' && i != (ssize_t)(total - 1)) {
            char *rev = malloc(line_len + 1);
            for(size_t j = 0; j < line_len; j++) rev[j] = line_buf[line_len - 1 - j];
            rev[line_len] = '\0';
            process_line(rev, line_len, number_lines, &rev_num, 1);
            free(rev);
            line_len = 0;
            free(line_buf);
            line_buf = NULL;
        } else if (full[i] != '\n') {
            line_buf = realloc(line_buf, line_len + 1);
            line_buf[line_len++] = full[i];
        }
    }
    if (line_len > 0) {
        char *rev = malloc(line_len + 1);
        for(size_t j = 0; j < line_len; j++) rev[j] = line_buf[line_len - 1 - j];
        rev[line_len] = '\0';
        process_line(rev, line_len, number_lines, &rev_num, 1);
        free(rev);
        free(line_buf);
    }
    free(full);
    if (number_lines) *line_num += non_empty_count;
}

void execute_peek(token tokens[], int count) {
    int num_lines = 0;
    int reverse = 0;
    char *files[512];
    int file_count = 0;

    for (int i = 1; i < count; i++) {
        if (tokens[i].values[0] == '-' && strlen(tokens[i].values) > 1 && strcmp(tokens[i].values, "-") != 0) {
            for (int j = 1; j < strlen(tokens[i].values); j++) {
                if (tokens[i].values[j] == 'n') num_lines = 1;
                else if (tokens[i].values[j] == 'r') reverse = 1;
            }
        } else {
            files[file_count++] = tokens[i].values;
        }
    }

    if (file_count == 0) {
        files[0] = "-";
        file_count = 1;
    }

    int line_num = 1;

    for (int i = 0; i < file_count; i++) {
        int fd;
        if (strcmp(files[i], "-") == 0) {
            fd = STDIN_FILENO;
        } else {
            struct stat st;
            if (stat(files[i], &st) == 0 && S_ISDIR(st.st_mode)) {
                printf("peek: is a directory\n");
                continue;
            }
            fd = open(files[i], O_RDONLY);
            if (fd < 0) {
                printf("peek: no such file or directory\n");
                continue;
            }
        }

        if (reverse) {
            if (lseek(fd, 0, SEEK_CUR) != -1) {
                peek_reverse_seekable(fd, num_lines, &line_num);
            } else {
                peek_reverse_stream(fd, num_lines, &line_num);
            }
        } else {
            peek_normal(fd, num_lines, &line_num);
        }

        if (fd != STDIN_FILENO) close(fd);
    }
}