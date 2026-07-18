#include "cc_common.h"
#include "memory/cc_dynamic_pool.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <utils.h>
#define ARENA_IMPLEMENTATION
#include <arena.h>

void initGrowingBuffer(GrowingBuffer *buffer, CC_DynamicPool *pool, int capacity)
{
    buffer->capacity = capacity;
    buffer->pool = pool;
    buffer->size = 0;
    buffer->ptr = cc_dynamic_pool_malloc(capacity, pool);
}
void appendGrowingBuffer(GrowingBuffer *buffer, char *src, ssize_t size)
{
    int newSize = (int)size + buffer->size;
    if (buffer->capacity <= newSize)
    {
        do
        {
            buffer->capacity <<= 1;
        } while (buffer->capacity <= newSize);
        char *new_ptr = cc_dynamic_pool_malloc(buffer->capacity, buffer->pool);
        memcpy(new_ptr, buffer->ptr, buffer->size);
        buffer->ptr = new_ptr;
    }
    memcpy(buffer->ptr + buffer->size, src, size);
    buffer->size = newSize;
}


void initString(String *s)
{
    bzero(s, sizeof(String));
}

void freeString(String *s)
{
    if (s->buf)
        free(s->buf);
}

void copyString(String *dst, char *src)
{
    freeString(dst);
    dst->length = strlen(src);
    dst->buf = malloc(dst->length);
    strcpy(dst->buf, src);
}


void initQueue(Queue *queue, size_t size)
{
    queue->size = 0;
    queue->tail = NULL;
    queue->head = NULL;
    queue->typeSize = size;
}
void freeQueue(Queue *queue)
{
    Node *current = queue->head;
    for (int i = 0; i < queue->size; i++)
    {
        free(current->data);
        Node *next = current->next;
        free(current);
        current = next;
    }
}
bool empty(Queue *queue)
{
    return queue->size == 0;
}
void push(Queue *queue, void *data)
{
    Node *node = malloc(sizeof(Node));
    node->data = data;
    node->next = NULL;
    if (queue->tail)
    {
        queue->tail->next = node;
        queue->tail = node;
    }
    else
    {
        queue->head = node;
        queue->tail = node;
    }
    queue->size++;
}
void *pop(Queue *queue)
{
    Node *head = queue->head;
    void *data = head->data;
    queue->head = head->next ? head->next : queue->tail;
    free(head);
    queue->size--;
    if (queue->size <= 0)
    {
        queue->head = NULL;
        queue->size = 0;
        queue->tail = NULL;
    }
    return data;
}
