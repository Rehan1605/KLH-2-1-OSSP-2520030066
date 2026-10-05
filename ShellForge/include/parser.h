#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "shell.h"
#include "tokenizer.h"

int parse_pipeline(const char *input, const Shell *shell, Pipeline *pipeline);
void pipeline_init(Pipeline *pipeline);
void pipeline_free(Pipeline *pipeline);
void pipeline_print(const Pipeline *pipeline);

#endif