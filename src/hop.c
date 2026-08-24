#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../include/hop.h"

typedef struct FrecencyEntry {
    char path[1024];
    int freq;
    long last_access;
} FrecencyEntry ;

void update_frecency(char *home_dir, char *hopped_path) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/.cshell_frecency", home_dir);
    
    FILE *f = fopen(file_path, "r");
    FrecencyEntry entries[1000];
    int entry_count = 0;
    int found = 0;
    long now = (long)time(NULL);
    if (f) {
        while (fscanf(f, "%1023s %d %ld", entries[entry_count].path, &entries[entry_count].freq, &entries[entry_count].last_access) == 3) {
            if (strcmp(entries[entry_count].path, hopped_path) == 0) {
                entries[entry_count].freq++;
                entries[entry_count].last_access = now;
                found = 1;
            }
            entry_count++;
        }
        fclose(f);
    }
    
    if (!found && entry_count < 1000) {
        strcpy(entries[entry_count].path, hopped_path);
        entries[entry_count].freq = 1;
        entries[entry_count].last_access = now;
        entry_count++;
    }

    f = fopen(file_path, "w");
    if (f) {
        for (int i = 0; i < entry_count; i++) {
            fprintf(f, "%s %d %ld\n", entries[i].path, entries[i].freq, entries[i].last_access);
        }
        fclose(f);
    }
}

int frecency_lookup(char *home_dir, char *name) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/.cshell_frecency", home_dir);
    
    FILE *f = fopen(file_path, "r");
    if (!f) return 0; // No history exists yet
    
    FrecencyEntry matches[1000];
    int match_count = 0;
    
    char temp_path[1024];
    int temp_freq;
    long temp_time;
    
    while (fscanf(f, "%1023s %d %ld", temp_path, &temp_freq, &temp_time) == 3) {
        if (strstr(temp_path, name) != NULL) {
            strcpy(matches[match_count].path, temp_path);
            matches[match_count].freq = temp_freq;
            matches[match_count].last_access = temp_time;
            match_count++;
        }
    }
    fclose(f);
    
    if (match_count == 0) return 0;
    
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = 0; j < match_count - i - 1; j++) {
            if (matches[j].freq < matches[j+1].freq || 
               (matches[j].freq == matches[j+1].freq && matches[j].last_access < matches[j+1].last_access)) {
                
                FrecencyEntry temp = matches[j];
                matches[j] = matches[j+1];
                matches[j+1] = temp;
            }
        }
    }

    for (int i = 0; i < match_count; i++) {
        if (chdir(matches[i].path) == 0) {
            return 1; 
        }
    }
    return 0; 
}

void execute_hop(token tokens[], int count, char *home_dir, char *prev_dir) {
    char current_before_hop[1024];
    char new_curr[1024];

    if (count == 1) {
        getcwd(current_before_hop, sizeof(current_before_hop));
        if (chdir(home_dir) == 0) {
            strcpy(prev_dir, current_before_hop); 
            getcwd(new_curr, sizeof(new_curr));
            update_frecency(home_dir, new_curr);
        }
        return;
    }

    for (int i = 1; i < count; i++) {
        char *arg = tokens[i].values;
        getcwd(current_before_hop, sizeof(current_before_hop));
        
        int success = 0;

        if (strcmp(arg, "~") == 0) {
            if (chdir(home_dir) == 0) success = 1;
        } 
        else if (strcmp(arg, "-") == 0) {
            if (strlen(prev_dir) == 0) {
                continue; 
            }
            if (chdir(prev_dir) == 0) success = 1;
        } 
        else if (strcmp(arg, ".") == 0) {
            success = 1; 
        } 
        else if (strcmp(arg, "..") == 0) {
            if (chdir("..") == 0) success = 1;
        } 
        else {
            if (chdir(arg) == 0) {
                success = 1;
            } 
            else {
                if (frecency_lookup(home_dir, arg)) {
                    success = 1;
                } else {
                    printf("hop: no such directory\n"); 
                }
            }
        }

        if (success) {
            strcpy(prev_dir, current_before_hop); 
            getcwd(new_curr, sizeof(new_curr));
            update_frecency(home_dir, new_curr);
        }
    }
}