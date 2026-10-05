#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.h"

#define INITIAL_TOKEN_CAPACITY 16

typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} WordBuffer;

static int token_list_grow(TokenList *list)
{
    if (list->count < list->capacity)
        return 1;

    int new_capacity = list->capacity == 0 ? INITIAL_TOKEN_CAPACITY : list->capacity * 2;
    Token *new_items = realloc(list->items, (size_t)new_capacity * sizeof(Token));
    if (new_items == NULL)
    {
        perror("realloc");
        return 0;
    }

    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

static int append_token(TokenList *list, TokenType type, const char *value)
{
    if (!token_list_grow(list))
        return 0;

    char *copy = strdup(value);
    if (copy == NULL)
    {
        perror("strdup");
        return 0;
    }
    list->items[list->count].type = type;
    list->items[list->count].value = copy;
    list->items[list->count].expanded = false;
    list->count++;
    return 1;
}

static int word_append(WordBuffer *word, const char *value, size_t length)
{
    if (length > SIZE_MAX - word->length - 1)
        return 0;

    size_t required = word->length + length + 1;
    if (required > word->capacity)
    {
        size_t new_capacity = word->capacity == 0 ? 64 : word->capacity;
        while (new_capacity < required)
        {
            if (new_capacity > SIZE_MAX / 2)
            {
                new_capacity = required;
                break;
            }
            new_capacity *= 2;
        }
        char *new_data = realloc(word->data, new_capacity);
        if (new_data == NULL)
        {
            perror("realloc");
            return 0;
        }
        word->data = new_data;
        word->capacity = new_capacity;
    }

    memcpy(word->data + word->length, value, length);
    word->length += length;
    word->data[word->length] = '\0';
    return 1;
}

static int word_append_char(WordBuffer *word, char value)
{
    return word_append(word, &value, 1);
}

static int append_variable(WordBuffer *word, const char **cursor, const Shell *shell)
{
    const char *start = *cursor + 1;
    const char *end = start;
    bool braced = false;

    if (*start == '{')
    {
        braced = true;
        start++;
        end = start;
    }

    while (isalnum((unsigned char)*end) || *end == '_')
        end++;

    if (end == start || (braced && *end != '}'))
        return word_append_char(word, *(*cursor)++);

    char *key = strndup(start, (size_t)(end - start));
    if (key == NULL)
    {
        perror("strndup");
        return 0;
    }
    const char *value = shell_getenv(shell, key);
    free(key);
    if (value != NULL && !word_append(word, value, strlen(value)))
        return 0;

    *cursor = braced ? end + 1 : end;
    return 1;
}

static int is_operator_char(char c)
{
    return c == '|' || c == '&' || c == '<' || c == '>';
}

static int parse_word(const char **cursor, TokenList *list, const Shell *shell)
{
    WordBuffer word = {0};
    bool in_single = false;
    bool in_double = false;
    bool started = false;

    while (**cursor != '\0')
    {
        char c = **cursor;

        if (c == '\\' && !in_single)
        {
            const char *next = *cursor + 1;
            if (*next == '\0')
            {
                fprintf(stderr, "ShellForge: syntax error: trailing escape\n");
                free(word.data);
                return 0;
            }
            if (in_double && *next != '$' && *next != '"' && *next != '\\' && *next != '\n')
            {
                if (!word_append_char(&word, c))
                    goto allocation_error;
                (*cursor)++;
                continue;
            }
            *cursor += 2;
            if (*next != '\n' && !word_append_char(&word, *next))
                goto allocation_error;
            started = true;
            continue;
        }

        if (c == '\'' && !in_double)
        {
            in_single = !in_single;
            started = true;
            (*cursor)++;
            continue;
        }

        if (c == '"' && !in_single)
        {
            in_double = !in_double;
            started = true;
            (*cursor)++;
            continue;
        }

        if (!in_single && c == '$')
        {
            started = true;
            if (!append_variable(&word, cursor, shell))
                goto allocation_error;
            continue;
        }

        if (!in_single && !in_double &&
            (isspace((unsigned char)c) || is_operator_char(c)))
            break;

        if (!word_append_char(&word, c))
            goto allocation_error;
        started = true;
        (*cursor)++;
    }

    if (in_single || in_double)
    {
        fprintf(stderr, "ShellForge: syntax error: unmatched quote\n");
        free(word.data);
        return 0;
    }

    if (started)
    {
        if (word.data == NULL)
        {
            word.data = strdup("");
            if (word.data == NULL)
                goto allocation_error;
        }
        int result = append_token(list, TOK_WORD, word.data);
        free(word.data);
        return result;
    }

    free(word.data);
    return 1;

allocation_error:
    free(word.data);
    return 0;
}

void token_list_init(TokenList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int tokenize(const char *input, TokenList *list, const Shell *shell)
{
    if (input == NULL || list == NULL)
        return 0;

    const char *cursor = input;
    while (*cursor != '\0')
    {
        while (*cursor != '\0' && isspace((unsigned char)*cursor))
            cursor++;

        if (*cursor == '\0')
            break;

        if (*cursor == '|')
        {
            if (!append_token(list, TOK_PIPE, "|"))
                return 0;
            cursor++;
            continue;
        }

        if (*cursor == '&')
        {
            if (!append_token(list, TOK_AMPERSAND, "&"))
                return 0;
            cursor++;
            continue;
        }

        if (cursor[0] == '2' && cursor[1] == '>')
        {
            TokenType type = cursor[2] == '>' ? TOK_REDIRECT_ERR_APPEND : TOK_REDIRECT_ERR;
            if (!append_token(list, type, type == TOK_REDIRECT_ERR_APPEND ? "2>>" : "2>"))
                return 0;
            cursor += type == TOK_REDIRECT_ERR_APPEND ? 3 : 2;
            continue;
        }

        if (strncmp(cursor, ">>", 2) == 0)
        {
            if (!append_token(list, TOK_REDIRECT_APPEND, ">>"))
                return 0;
            cursor += 2;
            continue;
        }

        if (*cursor == '>')
        {
            if (!append_token(list, TOK_REDIRECT_OUT, ">"))
                return 0;
            cursor++;
            continue;
        }

        if (*cursor == '<')
        {
            if (!append_token(list, TOK_REDIRECT_IN, "<"))
                return 0;
            cursor++;
            continue;
        }

        const char *word_start = cursor;
        if (!parse_word(&cursor, list, shell))
            return 0;
        if (cursor == word_start)
            return 0;
    }

    return 1;
}

void token_list_print(const TokenList *list)
{
    if (list == NULL)
        return;

    printf("Tokens (%d):\n", list->count);
    for (int i = 0; i < list->count; i++)
        printf("  [%d] type=%d value=%s\n", i, list->items[i].type, list->items[i].value);
}

void token_list_free(TokenList *list)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++)
        free(list->items[i].value);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}