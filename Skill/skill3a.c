#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX 10

int main()
{
    char *history[MAX];
    char command[100];
    int count = 0;

    while (1)
    {
        printf("Rehan$> ");
        fgets(command, sizeof(command), stdin);

        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "exit") == 0)
            break;

        if (strcmp(command, "history") == 0)
        {
            for (int i = 0; i < count; i++)
                printf("%d: %s\n", i + 1, history[i]);

            continue;
        }

        if (count < MAX)
        {
            history[count] = malloc(strlen(command) + 1);
            strcpy(history[count], command);
            count++;
        }
    }

    for (int i = 0; i < count; i++)
        free(history[i]);

    return 0;
}
