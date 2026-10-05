#ifndef INPUT_H
#define INPUT_H

#define INITIAL_BUFFER_SIZE 64

#include "shell.h"

int read_input(char **buffer, Shell *shell);

#endif