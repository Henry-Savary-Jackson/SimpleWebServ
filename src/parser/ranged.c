
#include "cc_array.h"
#include "cc_common.h"
#include "cc_list.h"
#include "cc_queue.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


// decode a string defining a byte range, using information on the requested file to determine if it is a valid range
int decodeRange(char *rangeStr, struct stat *file_stat, long *start, long *end)
{
    // try format "<start>-<end>""
    int count = sscanf(rangeStr, " %ld-%ld ", start, end);
    if (count == 0)
    {
        // doesnt work
        // try only one value, that being the negative start value
        int count = sscanf(rangeStr, " %ld ", end);
        if (count < 1)
        {
            return -1;
        }
        // make sure that the end value is only negative in thsi case
        assert(*end < 0);
        if (*end < 0)
        {
            return -2;
        }
        // set the start and end as absolute byte values
        *start = file_stat->st_size + *end;
        if (*start < 0)
        {
            return -1;
        }
        *end = file_stat->st_size;
        return 0;
        //
    }

    // if only the start was found (i.e the format is "<start>-"), the set the end as the end of the file

    if (count == 1)
    {
        *end = file_stat->st_size;
    }

    // make sure the start and end fit in the file
    if (*start >= file_stat->st_size)
    {
        return -1;
    }
    if (*end > file_stat->st_size)
    {
        return -1;
    }

    return 0;
}

int compRange(const void *elem1, const void *elem2)

{
    const Range *range = (const Range *)elem1;
    const Range *other = (const Range *)elem2;

    return range->start < other->start ? -1 : (range->start == other->start ? 0 : 1);
}

int decodeRangeHandle(char *inStr, void *arg)
{
    struct
    {
        CC_Array *ranges;
        struct stat *file_stat;
    } *args = arg;


    Range *range = custom_alloc(sizeof(Range));
    int ret = decodeRange(inStr, args->file_stat, (long *)&range->start, (long *)&range->end);
    if (ret < 0)
    {
        return ret;
    }
    cc_array_add(args->ranges, range);
    return 0;
}

int decodeRanges(char *inStr, struct stat *fstat, CC_Array* arr)
{
    char out[strlen(inStr) + 1];
    int count = sscanf(inStr, " bytes=%s ", out);
    assert(count == 1);

    struct
    {
        CC_Array *ranges;
        struct stat *fstat;
    } args = {.ranges = arr, .fstat = fstat};

    int ret_tokenize = tokenize(out, ',', decodeRangeHandle, &args);
    if (ret_tokenize < 0){
        return ret_tokenize;
    }

    cc_array_sort(arr, compRange);


    CC_ArrayIter iter;
    cc_array_iter_init(&iter, arr);
    Range *finalRange = NULL;
    Range *current = NULL;

    while (cc_array_iter_next(&iter, (void **)&current) != CC_ITER_END)
    {
        if (finalRange != NULL)
        {
            bool startInRange = (bool)(finalRange->start <= current->start && current->start <= finalRange->end);
            if (startInRange)
            {
                finalRange->end = MAX(current->end, finalRange->end);
                cc_array_iter_remove(&iter, (void **)&current);
                iter.index -= 1; // iter doesnt do this!!!!
                continue;
            }
        }
        // reset to newest once since there is no overlap
        finalRange = current;
    }
    return  0;
}
