#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "lexer.h"
#include "parser.h"

void execute_pipeline(token tokens[], int count, char *home_dir, char *prev_dir);

#endif