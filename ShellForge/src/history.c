#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

void history_init(History *history)
{
    history->head = NULL;
    history->tail = NULL;
    history->current = NULL;
}

void history_add(History *history, const char *command)
{
    if (command == NULL || command[0] == '\0')
        return;

    HistoryNode *node = malloc(sizeof(HistoryNode));

    if (node == NULL)
    {
        perror("malloc");
        return;
    }

    node->command = malloc(strlen(command) + 1);

    if (node->command == NULL)
    {
        perror("malloc");
        free(node);
        return;
    }

    strcpy(node->command, command);

    node->next = NULL;
    node->prev = history->tail;

    if (history->tail != NULL)
        history->tail->next = node;
    else
        history->head = node;

    history->tail = node;
    history->current = NULL;
}

const char *history_previous(History *history)
{
    if (history->tail == NULL)
        return NULL;

    if (history->current == NULL)
        history->current = history->tail;
    else if (history->current->prev != NULL)
        history->current = history->current->prev;

    return history->current->command;
}

const char *history_next(History *history)
{
    if (history->current == NULL)
        return NULL;

    if (history->current->next != NULL)
    {
        history->current = history->current->next;
        return history->current->command;
    }

    history->current = NULL;
    return "";
}

void history_reset_navigation(History *history)
{
    history->current = NULL;
}

void history_print(const History *history)
{
    const HistoryNode *current = history->head;
    int number = 1;

    while (current != NULL)
    {
        printf("%d  %s\n", number++, current->command);
        current = current->next;
    }
}

void history_free(History *history)
{
    HistoryNode *current = history->head;

    while (current != NULL)
    {
        HistoryNode *next = current->next;

        free(current->command);
        free(current);

        current = next;
    }

    history->head = NULL;
    history->tail = NULL;
    history->current = NULL;
}