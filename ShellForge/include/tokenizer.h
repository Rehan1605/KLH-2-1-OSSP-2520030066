#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdbool.h>

#include "shell.h"

typedef enum
{
    TOK_WORD = 0,
    TOK_PIPE = 1,
    TOK_REDIRECT_IN = 2,
    TOK_REDIRECT_OUT = 3,
    TOK_REDIRECT_APPEND = 4,
    TOK_REDIRECT_ERR = 5,
    TOK_AMPERSAND = 6,
    TOK_REDIRECT_ERR_APPEND = 7
} TokenType;

typedef struct
{
    char *value;
    TokenType type;
    bool expanded;
} Token;

typedef struct
{
    Token *items;
    int count;
    int capacity;
} TokenList;

void token_list_init(TokenList *list);
int tokenize(const char *input, TokenList *list, const Shell *shell);
void token_list_print(const TokenList *list);
void token_list_free(TokenList *list);

#endif