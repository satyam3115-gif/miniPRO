#ifndef LEXER_H
#define LEXER_H

typedef enum {
    OP_PIPE,   
    OP_AMP,    
    OP_SEMI, 
    OP_LT,     
    OP_GT,     
    OP_GTGT,   
    WORD       
} token_type;

typedef struct token{
    token_type type;
    char values[1024];
} token;

int lexer(char *input, token tokens[]);

#endif