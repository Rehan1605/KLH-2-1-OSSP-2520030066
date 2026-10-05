#ifndef HISTORY_H
#define HISTORY_H

typedef struct HistoryNode
{
    char *command;
    struct HistoryNode *next;
    struct HistoryNode *prev;
} HistoryNode;

typedef struct
{
    HistoryNode *head;
    HistoryNode *tail;
    HistoryNode *current;
} History;

void history_init(History *history);
void history_add(History *history, const char *command);
const char *history_previous(History *history);
const char *history_next(History *history);
void history_reset_navigation(History *history);
void history_print(const History *history);
void history_free(History *history);

#endif