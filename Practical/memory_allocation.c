#include <stdio.h>
#include <stdlib.h>

int main()
{
    int *a, *b;

    // malloc(): allocate memory for 5 integers
    a = (int *)malloc(5 * sizeof(int));

    if (a == NULL)
    {
        printf("malloc failed\n");
        return 1;
    }

    // calloc(): allocate memory for 5 integers and initialize to zero
    b = (int *)calloc(5, sizeof(int));

    if (b == NULL)
    {
        printf("calloc failed\n");
        free(a);
        return 1;
    }

    printf("Memory allocated using malloc() and calloc().\n");

    // realloc(): resize memory allocated by malloc()
    a = (int *)realloc(a, 10 * sizeof(int));

    if (a == NULL)
    {
        printf("realloc failed\n");
        free(b);
        return 1;
    }

    printf("Memory resized using realloc().\n");

    // free(): release allocated memory
    free(a);
    free(b);

    printf("Memory freed successfully.\n");

    return 0;
}
