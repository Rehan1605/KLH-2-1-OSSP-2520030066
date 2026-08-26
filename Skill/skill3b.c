#include <stdio.h>
#include <string.h>

int main()
{
    char input[100];
    char *token;

    printf("Enter command: ");
    fgets(input, sizeof(input), stdin);

    input[strcspn(input, "\n")] = '\0';

    if (strlen(input) == 0)
    {
        printf("Empty command\n");
        return 0;
    }

    token = strtok(input, " ");

    while (token != NULL)
    {
        printf("Token: %s\n", token);
        token = strtok(NULL, " ");
    }

    return 0;
}
