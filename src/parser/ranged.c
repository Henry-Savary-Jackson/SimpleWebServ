
#include "cc_queue.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#define CHUNK_SIZE 1 << 14

int decodeRange(char *rangeStr, struct stat *file_stat, long *start, long *end)
{


    int count = sscanf(rangeStr, " bytes=%ld-%ld ", start, end);
    if (count == 0)
    {
        //
        int count = sscanf(rangeStr, " bytes=%ld ", end);
        if (count < 1)
        {
            return -1;
        }
        assert(*end < 0);
        if (*end < 0)
        {
            return -2;
        }
        *start = file_stat->st_size + *end;
        *start = *start < 0 ? 0 : *start;
        *end = file_stat->st_size;
        return 0;
        //
    }
    if (count == 1)
    {
        *end = *start + (CHUNK_SIZE);
    }

    *start = *start > file_stat->st_size ? file_stat->st_size : *start;
    *end = *end > file_stat->st_size ? file_stat->st_size : *end;
    return 0;
}

void decodeRangeHandle(char *inStr, void *arg)
{
    struct
    {
        CC_Queue *queue;
        struct stat *file_stat;
    } *args = arg;

    Range *range = custom_alloc(sizeof(Range));
    decodeRange(inStr, args->file_stat, (long *)&range->start, (long *)&range->end);
    cc_queue_enqueue(args->queue, range);
}

CC_Queue *decodeRanges(char *inStr, struct stat *fstat)
{
    CC_Queue *queue;
    CC_QueueConf conf;
    cc_queue_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;
    cc_queue_new_conf(&conf, &queue);

    struct
    {
        CC_Queue *queue;
        struct stat *fstat;
    } args = {.queue = queue, .fstat = fstat};

    tokenize(inStr, ',', decodeRangeHandle, &args);

    return queue;
}
