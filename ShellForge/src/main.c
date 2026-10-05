#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "shell.h"

int main(void)
{
    Shell shell;
    shell_init(&shell);

    while (!shell.exit_requested)
    {
        shell_print_prompt(&shell);

        char *input = NULL;
        if (!read_input(&input, &shell))
        {
            shell.exit_requested = true;
            break;
        }

        if (input == NULL || input[0] == '\0')
        {
            free(input);
            continue;
        }

        shell_execute_line(&shell, input);
    }

    shell_free(&shell);
    return shell.last_status;
}