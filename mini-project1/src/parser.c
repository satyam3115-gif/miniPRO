#include <stdio.h>
#include "../include/parser.h"

int parse(token tokens[], int count) {
    if(tokens[0].type != WORD) return -1; 
    
    for (int i=1; i<count; i++) {
        if (tokens[i].type == WORD) {
            continue; 
        }else if(tokens[i].type == OP_PIPE || tokens[i].type == OP_SEMI || tokens[i].type == OP_LT || tokens[i].type == OP_GT || tokens[i].type == OP_GTGT) {
            i++;
            if(i == count || tokens[i].type != WORD) {
                return -1; 
            }
        } 
        else if(tokens[i].type == OP_AMP) {
            if(i+1 < count) {
                  i++; 
                if(tokens[i].type != WORD) {
                    return -1;
                }
            }
        }
    }
    return 1; 
}
