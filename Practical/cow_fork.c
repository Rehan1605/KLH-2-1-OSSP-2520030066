#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    int x = 100;

    printf("Before fork: x = %d\n", x);

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return 1;
    }

    if (pid == 0)
    {
        // Child process
        printf("Child Before = %d\n", x);

        x = 500;

        printf("Child After  = %d\n", x);
    }
    else
    {
        // Parent process
        sleep(2);
        printf("Parent       = %d\n", x);

        wait(NULL);
    }

    return 0;
}
