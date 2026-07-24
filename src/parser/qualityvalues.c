#include "cc_common.h"
#include "cc_deque.h"
#include "cc_pqueue.h"
#include "server.h"
#include "utils.h"
#include <http.h>
#include <linux/limits.h>
#include <magic.h>
#include <netinet/in.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <zconf.h>
#include <zlib.h>
#include <zstd.h>




void decodeQualityValueString(char *qvString, CC_PQueue *pqueue, void *(*decodeFunc)(char *))
{
    int strLen = strlen(qvString);
    char currentString[strLen];

    int index = 0;
    int consumedIndex = 0;
    int writeIndex = 0;
    while (index < strLen + 1)
    {
        char c = qvString[index];
        index++;
        if (c == ',' || c == 0)
        {
            currentString[writeIndex] = 0;
            // hande
            void *qv = decodeFunc(currentString);
            if (qv)
            {
                // error
                cc_pqueue_push(pqueue, qv);
            }
            writeIndex = 0;
            continue;
        }
        currentString[writeIndex] = c;
        writeIndex++;
    }
}


CC_PQueue *decodeQualityValues(CC_Deque *teList, void *(*decodeFunc)(char *), int (*cmp)(const void *, const void *))
{
    CC_PQueue *pqueue = NULL;
    CC_PQueueConf conf;
    cc_pqueue_conf_init(&conf, cmp);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;

    cc_pqueue_new_conf(&conf, &pqueue);

    CC_DequeIter iter;
    cc_deque_iter_init(&iter, teList);
    enum cc_stat stat;
    char *str;
    while ((stat = cc_deque_iter_next(&iter, (void **)&str)) != CC_ITER_END)
    {
        decodeQualityValueString(str, pqueue, decodeFunc);
    }
    return pqueue;
}
