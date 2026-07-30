#include "arena.h"
#include "memory/cc_dynamic_pool.h"
#include "server.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils.h>

void initGrowingBuffer(GrowingBuffer *buffer, int capacity)
{
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->ptr = custom_alloc(capacity);
}
void appendGrowingBuffer(GrowingBuffer *buffer, char *src, size_t size)
{
    int newSize = (int)size + buffer->size;
    increaseCapacityGrowingBuffer(buffer, (int)size);
    memcpy(buffer->ptr + buffer->size, src, size);
    buffer->size = newSize;
}

void increaseCapacityGrowingBuffer(GrowingBuffer *buffer, int amount)
{
    int newSize = amount + buffer->size;
    if (buffer->capacity <= newSize)
    {
        do
        {
            buffer->capacity <<= 1;
        } while (buffer->capacity <= newSize);

        char *new_ptr = custom_alloc(buffer->capacity);
        memcpy(new_ptr, buffer->ptr, buffer->size);
        buffer->ptr = new_ptr;
    }
}


void initString(String *s)
{
    memset(s, 0, sizeof(String));
}

void freeString(String *s)
{
    if (s->buf)
    {
        free(s->buf);
    }
}

void copyString(String *dst, char *src)
{
    freeString(dst);
    dst->length = (int)strlen(src);
    dst->buf = malloc(dst->length);
    strcpy(dst->buf, src);
}

void copyStringToPool(char **dst, char *src, CC_DynamicPool *pool)
{
    *dst = cc_dynamic_pool_malloc(strlen(src) + 1, pool);
    strcpy(*dst, src);
}

// we assume the last enum is for unknown
int arrLineSearch(char **arr, int size, char *key)
{
    int i = 0;
    while (i < size - 1)
    {
        if (!strcmp(key, arr[i]))
        {
            return i;
        }
        i++;
    }
    return i;
}
inline char separator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

void tokenize(char *inStr,char token, void (*handle)(char *, void*), void* args)
{
    int strLen = strlen(inStr);
    char currentString[strLen];

    int index = 0;
    int consumedIndex = 0;
    int writeIndex = 0;
    while (index < strLen + 1)
    {
        char c = inStr[index];
        index++;
        if (c == token || c == 0)
        {
            currentString[writeIndex] = 0;
            handle(currentString, args);
            writeIndex = 0;
            continue;
        }
        currentString[writeIndex] = c;
        writeIndex++;
    }
}

void strToLower(char * str, char** out ){
    int len = (int)strlen(str);
    *out = custom_alloc(len+1);
    for (int i = 0; i < len; i ++){
        (*out)[i] = (char) tolower((int)str[i]);
    }
}
