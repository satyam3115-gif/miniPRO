#include <stdio.h>
#include <string.h>
#include "../include/lexer.h"

int lexer(char *input, token tokens[]) {
    int i=0;
    int count = 0;

    while(input[i] != '\0') {
        if(input[i] == '\t' || input[i] == '\r' || input[i] == '\n' || input[i] == ' ') {
            i++;
            continue;
        }
        if(input[i] == '>') {
            if(input[i+1] == '>'){
                tokens[count].type = OP_GTGT;
                strcpy(tokens[count].values, ">>");
                i+=2;
            }else {
                tokens[count].type = OP_GT;
                strcpy(tokens[count].values, ">");
                i++;
            }
            count++;
            continue;
        }
        if(input[i] == '<') {
            tokens[count].type = OP_LT;
            strcpy(tokens[count].values, "<");
            count++;
            i++;
            continue;
        }
        if(input[i] == '|') {
            tokens[count].type = OP_PIPE;
            strcpy(tokens[count].values, "|");
            count++;
            i++;
            continue;
        }
        if(input[i] == '&') {
            tokens[count].type = OP_AMP;
            strcpy(tokens[count].values, "&");
            count++;
            i++;
            continue;
        }
        if(input[i] == ';') {
            tokens[count].type = OP_SEMI;
            strcpy(tokens[count].values, ";");
            count++;
            i++;
            continue;
        }

        char word[1024];
        int idx=0, in_single=0, in_double=0;

        while(input[i] != '\0') {
            if(!in_single && !in_double){
                if(input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r') {
                    break;
                }
                if(input[i] == '<' || input[i] == '>' || input[i] == '|' || input[i] == '&' || input[i] == ';'){
                    break;
                }
            }
            if(input[i] == '\\' && !in_single) {
                if(input[i+1] == '\0') {
                    printf("cshell: invalid syntax\n");
                    return -1;
                }
                if(in_double) {
                    if(input[i+1] == '"' || input[i+1] == '\\') {
                        i++;
                        word[idx++] = input[i];
                    }else {
                        word[idx++] = input[i];
                        i++;
                        word[idx++] = input[i];
                    }
                }else {
                    i++;
                    word[idx++] = input[i];
                }
            }else if(input[i] == '\'') {
                if(!in_double) in_single = !in_single;
                else word[idx++] = input[i];
            }else if(input[i] == '"') {
                if(!in_single) in_double = !in_double;
                else word[idx++] = input[i];
            }else {
                word[idx++] = input[i];
            }
            i++;
        }
        if (in_single || in_double) {
            printf("cshell: invalid syntax\n");
            return -1;
        }
        word[idx] = '\0';
        tokens[count].type = WORD;
        strcpy(tokens[count].values, word);
        count++;
        }
        return count;
}
