#pragma once

#include <stddef.h>
typedef struct{char * buf; int length;} String;


typedef struct {
    void* data;
    void* next;
} Node;

typedef struct{
    int size;
    size_t typeSize;
    Node* head;
    Node* tail;
} Queue;

void initString(String* s);
void freeString(String* s);
void copyString(String* dst, char * src);
char* allocStringArena(void * dict, char * value);

void initQueue(Queue* queue, size_t size);
void freeQueue(Queue* queue);
bool empty(Queue* queue);
void push(Queue* queue, void* data);
void* pop(Queue* queue);
