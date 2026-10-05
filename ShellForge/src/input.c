#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "input.h"

static void clear_current_line(int length)
{
    for (int i = 0; i < length; i++)
        printf("\b \b");

    fflush(stdout);
}

static int resize_buffer(char **buffer, int *capacity)
{
    if (*capacity > INT_MAX / 2)
    {
        fprintf(stderr, "ShellForge: input line is too long\n");
        return 0;
    }
    int new_capacity = (*capacity) * 2;

    char *new_buffer = realloc(*buffer, new_capacity);

    if (new_buffer == NULL)
    {
        perror("realloc");
        return 0;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;

    return 1;
}

static void replace_buffer(
    char **buffer,
    int *length,
    int *capacity,
    const char *replacement)
{
    int required = strlen(replacement) + 1;

    while (*capacity < required)
    {
        if (!resize_buffer(buffer, capacity))
            return;
    }

    clear_current_line(*length);

    strcpy(*buffer, replacement);
    *length = strlen(replacement);

    printf("%s", *buffer);
    fflush(stdout);
}

int read_input(char **buffer, Shell *shell)
{
    if (buffer == NULL || shell == NULL)
        return 0;
    *buffer = NULL;
    History *history = &shell->history;

    if (!isatty(STDIN_FILENO))
    {
        size_t capacity = 0;
        ssize_t length;
        do
        {
            errno = 0;
            length = getline(buffer, &capacity, stdin);
            if (length < 0 && errno == EINTR)
            {
                clearerr(stdin);
                shell_reap_jobs(shell);
            }
        } while (length < 0 && errno == EINTR);
        if (length < 0)
        {
            free(*buffer);
            *buffer = NULL;
            return 0;
        }
        if (length > 0 && (*buffer)[length - 1] == '\n')
            (*buffer)[--length] = '\0';
        if (length > 0 && (*buffer)[length - 1] == '\r')
            (*buffer)[--length] = '\0';
        history_reset_navigation(history);
        return 1;
    }

    struct termios old_settings;
    struct termios new_settings;

    if (tcgetattr(STDIN_FILENO, &old_settings) == -1)
        return 0;

    new_settings = old_settings;
    new_settings.c_lflag &= ~(ICANON | ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) == -1)
        return 0;

    int capacity = INITIAL_BUFFER_SIZE;
    int length = 0;

    *buffer = malloc(capacity);

    if (*buffer == NULL)
    {
        perror("malloc");
        tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
        return 0;
    }

    (*buffer)[0] = '\0';

    while (1)
    {
        char c;

        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread < 0)
        {
            if (errno == EINTR)
            {
                shell_reap_jobs(shell);
                continue;
            }
            free(*buffer);
            tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            return 0;
        }

        if (nread == 0)
        {
            free(*buffer);
            tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
            return 0;
        }

        if (c == 4)
        {
            if (length == 0)
            {
                free(*buffer);
                tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
                *buffer = NULL;
                return 0;
            }
            (*buffer)[length] = '\0';
            printf("\n");
            break;
        }

        /* Enter */
        if (c == '\n' || c == '\r')
        {
            (*buffer)[length] = '\0';
            printf("\n");
            break;
        }

        /* Backspace */
        if (c == 127 || c == '\b')
        {
            if (length > 0)
            {
                length--;
                (*buffer)[length] = '\0';

                printf("\b \b");
                fflush(stdout);
            }

            continue;
        }

        /* Escape sequence */
        if (c == 27)
        {
            char sequence[2];

            if (read(STDIN_FILENO, &sequence[0], 1) != 1)
                continue;

            if (sequence[0] != '[')
                continue;

            if (read(STDIN_FILENO, &sequence[1], 1) != 1)
                continue;

            /* Up arrow */
            if (sequence[1] == 'A')
            {
                const char *previous = history_previous(history);

                if (previous != NULL)
                    replace_buffer(
                        buffer,
                        &length,
                        &capacity,
                        previous);
            }

            /* Down arrow */
            else if (sequence[1] == 'B')
            {
                const char *next = history_next(history);

                if (next != NULL)
                    replace_buffer(
                        buffer,
                        &length,
                        &capacity,
                        next);
            }

            continue;
        }

        /* Ensure enough space */
        if (length >= capacity - 1)
        {
            if (!resize_buffer(buffer, &capacity))
            {
                free(*buffer);
                tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
                return 0;
            }
        }

        (*buffer)[length++] = c;
        (*buffer)[length] = '\0';

        putchar(c);
        fflush(stdout);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

    history_reset_navigation(history);

    return 1;
}