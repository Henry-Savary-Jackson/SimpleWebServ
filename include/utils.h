#pragma once

#include "cc_deque.h"
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
    void* prev;
} Node;


typedef struct
{
    bool isRoot;
    CC_Deque* directories;
} Path;

typedef struct
{
    int capacity;
    int size;
    char *ptr;
} GrowingBuffer;

void initGrowingBuffer(GrowingBuffer *buffer, int capacity);
void appendGrowingBuffer(GrowingBuffer *buffer, char *src, size_t size);
void increaseCapacityGrowingBuffer(GrowingBuffer* buffer, int amount);

void initString(String *str);
void freeString(String *str);
void copyString(String *dst, char *src);
char *allocStringArena(void *dict, char *value);
void copyStringToPool(char ** dst, char* src, CC_DynamicPool* pool);
int arrLineSearch(char **arr, int size, char *key);

#define HTTP_METHOD(s)                                                                                                 \
    (enum http_method)(arrLineSearch((char **)http_method_arr, sizeof(http_method_arr) / sizeof(char *), (char *)(s)))
#define HTTP_ENCODING(s)                                                                                               \
    (enum http_encoding)(                                                                                              \
        arrLineSearch((char **)http_encoding_arr, sizeof(http_method_arr) / sizeof(char *), (char *)(s)))

#define HTTP_SUPPORTS_ENCODING(e) (bool)(                                                                                              \
        arrLineSearch((char **)http_encoding_arr, sizeof(http_method_arr) / sizeof(char *), (char *)(s)) == -1)

#define HTTP_CONTENT_TYPE(s) (enum http_content_type)(arrLineSearch(char**)http_content_type_arr, sizeof(http_method_arr)/sizeof(char *), ( char*)(s) ))

#define HTTP_METHOD_STRING(e) (char *)(http_method_arr[(int)(e)])
#define HTTP_ENCODING_STRING(e) (char *)(http_encoding_arr[(int)(e)])
#define HTTP_CONTENT_TYPE_STRING(e) (char *)(http_content_type_arr[(int)(e)])

inline char separator();

#ifdef _WIN32
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif

#ifdef _WIN32
    #define PATH_SEP_STR "\\"
#else
    #define PATH_SEP_STR "/"
#endif

void initPath(Path* path);
void stringToPath(Path* path,char * strPath);
void commonRoot(Path* output,Path *path1, Path *path2);
void concatenatePath(Path* prefix, Path* rest);
void pathToStr(Path *path, char* outBuffer);
void addToPath(Path* path, char* dirname);
void popFromPath(Path* path);
void removePrefix(Path *path, Path *prefix);
void sanitizePath(Path* path);
