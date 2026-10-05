#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void command_reset(Command *command)
{
    if (command == NULL)
        return;

    if (command->argv != NULL)
    {
        for (int i = 0; command->argv[i] != NULL; i++)
            free(command->argv[i]);
        free(command->argv);
    }

    for (size_t i = 0; i < command->redirection_count; i++)
        free(command->redirections[i].target);
    free(command->redirections);

    memset(command, 0, sizeof(*command));
}

static int command_add_arg(Command *command, const char *value)
{
    if (command == NULL || value == NULL)
        return 0;

    char **new_argv = realloc(command->argv, (size_t)(command->argc + 2) * sizeof(char *));
    if (new_argv == NULL)
    {
        perror("realloc");
        return 0;
    }

    command->argv = new_argv;
    command->argv[command->argc] = NULL;
    command->argv[command->argc] = strdup(value);
    if (command->argv[command->argc] == NULL)
    {
        perror("strdup");
        return 0;
    }
    command->argv[command->argc + 1] = NULL;
    command->argc++;
    return 1;
}

static int command_add_redirection(Command *command, RedirectionType type, const char *target)
{
    char *target_copy = target == NULL ? NULL : strdup(target);
    if (target != NULL && target_copy == NULL)
    {
        perror("strdup");
        return 0;
    }

    if (command->redirection_count == command->redirection_capacity)
    {
        size_t new_capacity = command->redirection_capacity == 0 ? 4 : command->redirection_capacity * 2;
        Redirection *new_redirections = realloc(command->redirections, new_capacity * sizeof(*new_redirections));
        if (new_redirections == NULL)
        {
            perror("realloc");
            free(target_copy);
            return 0;
        }
        command->redirections = new_redirections;
        command->redirection_capacity = new_capacity;
    }

    command->redirections[command->redirection_count].type = type;
    command->redirections[command->redirection_count].target = target_copy;
    command->redirection_count++;
    return 1;
}

void pipeline_init(Pipeline *pipeline)
{
    if (pipeline == NULL)
        return;

    pipeline->commands = NULL;
    pipeline->count = 0;
    pipeline->capacity = 0;
    pipeline->background = false;
}

static int pipeline_add(Pipeline *pipeline, Command *command)
{
    if (pipeline == NULL || command == NULL)
        return 0;

    if (pipeline->count >= pipeline->capacity)
    {
        int new_capacity = pipeline->capacity == 0 ? 4 : pipeline->capacity * 2;
        Command *new_commands = realloc(pipeline->commands, (size_t)new_capacity * sizeof(Command));
        if (new_commands == NULL)
        {
            perror("realloc");
            return 0;
        }
        pipeline->commands = new_commands;
        pipeline->capacity = new_capacity;
    }

    pipeline->commands[pipeline->count++] = *command;
    memset(command, 0, sizeof(*command));
    return 1;
}

void pipeline_free(Pipeline *pipeline)
{
    if (pipeline == NULL)
        return;

    for (int i = 0; i < pipeline->count; i++)
    {
        Command *command = &pipeline->commands[i];
        if (command->argv != NULL)
        {
            for (int j = 0; command->argv[j] != NULL; j++)
                free(command->argv[j]);
            free(command->argv);
        }
        for (size_t j = 0; j < command->redirection_count; j++)
            free(command->redirections[j].target);
        free(command->redirections);
    }

    free(pipeline->commands);
    pipeline->commands = NULL;
    pipeline->count = 0;
    pipeline->capacity = 0;
    pipeline->background = false;
}

void pipeline_print(const Pipeline *pipeline)
{
    if (pipeline == NULL)
        return;

    printf("Pipeline commands: %d background=%s\n", pipeline->count, pipeline->background ? "true" : "false");
    for (int i = 0; i < pipeline->count; i++)
    {
        printf("  command %d: ", i);
        for (int j = 0; pipeline->commands[i].argv != NULL && pipeline->commands[i].argv[j] != NULL; j++)
            printf("%s%s", pipeline->commands[i].argv[j], j + 1 < pipeline->commands[i].argc ? " " : "");
        printf("\n");
    }
}

static int finalize_command(Pipeline *pipeline, Command *command)
{
    if (command == NULL || command->argv == NULL || command->argv[0] == NULL)
        return 0;
    return pipeline_add(pipeline, command);
}

int parse_pipeline(const char *input, const Shell *shell, Pipeline *pipeline)
{
    if (input == NULL || pipeline == NULL)
        return 0;

    pipeline_init(pipeline);
    TokenList tokens;
    token_list_init(&tokens);
    if (!tokenize(input, &tokens, shell))
    {
        token_list_free(&tokens);
        return 0;
    }

    Command current;
    memset(&current, 0, sizeof(current));
    bool need_command = true;
    int i = 0;

    while (i < tokens.count)
    {
        Token *token = &tokens.items[i];

        if (token->type == TOK_WORD)
        {
            if (!command_add_arg(&current, token->value))
                goto parse_error;
            need_command = false;
            i++;
            continue;
        }

        if (token->type == TOK_PIPE)
        {
            if (need_command || current.argv == NULL)
            {
                fprintf(stderr, "ShellForge: syntax error near unexpected '|'\n");
                goto parse_error;
            }
            if (!finalize_command(pipeline, &current))
                goto parse_error;
            need_command = true;
            i++;
            continue;
        }

        if (token->type == TOK_REDIRECT_IN)
        {
            if (i + 1 >= tokens.count || tokens.items[i + 1].type != TOK_WORD)
            {
                fprintf(stderr, "ShellForge: syntax error near '<'\n");
                goto parse_error;
            }
            if (!command_add_redirection(&current, REDIR_INPUT, tokens.items[i + 1].value))
                goto parse_error;
            i += 2;
            continue;
        }

        if (token->type == TOK_REDIRECT_OUT)
        {
            if (i + 1 >= tokens.count || tokens.items[i + 1].type != TOK_WORD)
            {
                fprintf(stderr, "ShellForge: syntax error near '>'\n");
                goto parse_error;
            }
            if (!command_add_redirection(&current, REDIR_OUTPUT, tokens.items[i + 1].value))
                goto parse_error;
            i += 2;
            continue;
        }

        if (token->type == TOK_REDIRECT_APPEND)
        {
            if (i + 1 >= tokens.count || tokens.items[i + 1].type != TOK_WORD)
            {
                fprintf(stderr, "ShellForge: syntax error near '>>'\n");
                goto parse_error;
            }
            if (!command_add_redirection(&current, REDIR_APPEND, tokens.items[i + 1].value))
                goto parse_error;
            i += 2;
            continue;
        }

        if (token->type == TOK_REDIRECT_ERR || token->type == TOK_REDIRECT_ERR_APPEND)
        {
            if (token->type == TOK_REDIRECT_ERR && i + 2 < tokens.count &&
                tokens.items[i + 1].type == TOK_AMPERSAND && tokens.items[i + 2].type == TOK_WORD &&
                strcmp(tokens.items[i + 2].value, "1") == 0)
            {
                if (!command_add_redirection(&current, REDIR_ERROR_TO_OUTPUT, NULL))
                    goto parse_error;
                i += 3;
                continue;
            }
            if (i + 1 >= tokens.count || tokens.items[i + 1].type != TOK_WORD)
            {
                fprintf(stderr, "ShellForge: syntax error near stderr redirection\n");
                goto parse_error;
            }
            RedirectionType type = token->type == TOK_REDIRECT_ERR_APPEND ?
                                   REDIR_ERROR_APPEND : REDIR_ERROR;
            if (!command_add_redirection(&current, type, tokens.items[i + 1].value))
                goto parse_error;
            i += 2;
            continue;
        }

        if (token->type == TOK_AMPERSAND)
        {
            if (need_command || current.argv == NULL || i + 1 != tokens.count)
            {
                fprintf(stderr, "ShellForge: syntax error near unexpected '&'\n");
                goto parse_error;
            }
            pipeline->background = true;
            i++;
            break;
        }

        fprintf(stderr, "ShellForge: syntax error\n");
        goto parse_error;
    }

    if (need_command || current.argv == NULL || current.argv[0] == NULL)
    {
        fprintf(stderr, "ShellForge: syntax error: expected command\n");
        goto parse_error;
    }
    if (!finalize_command(pipeline, &current))
        goto parse_error;

    token_list_free(&tokens);
    return 1;

parse_error:
    token_list_free(&tokens);
    command_reset(&current);
    pipeline_free(pipeline);
    return 0;
}