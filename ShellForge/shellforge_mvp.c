#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 256
#define MAX_ARGS 20

int main()
{
    char input[MAX_INPUT];

    while (1)
    {
        printf("2520030066_SHELLFORGE$ ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
            continue;

        if (strcmp(input, "exit") == 0)
            break;

        // Tokenize command
        char *args[MAX_ARGS];
        int argc = 0;

        char *token = strtok(input, " ");

        while (token != NULL && argc < MAX_ARGS - 1)
        {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }

        args[argc] = NULL;

        // Create child process
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            continue;
        }

        if (pid == 0)
        {
            // Child executes command
            execvp(args[0], args);

            // Only reached if execvp fails
            perror("ShellForge");
            exit(1);
        }
        else
        {
            // Parent waits for child
            waitpid(pid, NULL, 0);
        }
    }

    printf("Exiting ShellForge...\n");

    return 0;
}
