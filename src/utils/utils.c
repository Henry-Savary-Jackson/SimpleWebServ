#include "memory/cc_dynamic_pool.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <utils.h>

void initGrowingBuffer(GrowingBuffer *buffer, CC_DynamicPool *pool, int capacity)
{
    buffer->capacity = capacity;
    buffer->pool = pool;
    buffer->size = 0;
    buffer->ptr = cc_dynamic_pool_malloc(capacity, pool);
}
void appendGrowingBuffer(GrowingBuffer *buffer, char *src, size_t size)
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
    memset(s,0, sizeof(String));
}

void freeString(String *s)
{
    if (s->buf) {
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

void copyStringToPool(char ** dst, char* src, CC_DynamicPool* pool){
    *dst= cc_dynamic_pool_malloc(strlen(src)+1, pool);
    strcpy(*dst, src);
}



int arrLineSearch(char** arr, int size, char* key){
    int i =0 ;
    while(i < size){
        if (!strcmp(key, arr[i])) break;
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
