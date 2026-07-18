#pragma once

#include "memory/cc_dynamic_pool.h"
#include <iso646.h>
#include <stddef.h>
#include <sys/types.h>
typedef struct
{
    char *buf;
    int length;
} String;

typedef struct
{
    void *data;
    void *next;
} Node;

typedef struct
{
    int size;
    size_t typeSize;
    Node *head;
    Node *tail;
} Queue;


typedef struct {
    int capacity;
    int size;
    char * ptr;
    CC_DynamicPool* pool;
} GrowingBuffer;

void initGrowingBuffer(GrowingBuffer* buffer,CC_DynamicPool* pool, int capacity);
void appendGrowingBuffer(GrowingBuffer* buffer, char* src, ssize_t size);

void initString(String *s);
void freeString(String *s);
void copyString(String *dst, char *src);
char *allocStringArena(void *dict, char *value);

void initQueue(Queue *queue, size_t size);
void freeQueue(Queue *queue);
bool empty(Queue *queue);
void push(Queue *queue, void *data);
void *pop(Queue *queue);
