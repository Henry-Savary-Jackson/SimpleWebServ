#include "cc_common.h"
#include "cc_deque.h"
#include "cc_list.h"
#include "cc_pqueue.h"
#include "cc_queue.h"
#include "server.h"
#include "utils.h"
#include <http.h>
#include <linux/limits.h>
#include <magic.h>
#include <netinet/in.h>
#include <parser.h>
#include <threads.h>
#include <zconf.h>
#include <zlib.h>
#include <zstd.h>


void *decodeSingleEncodingQualityValue(char *qvString)
{
    float q_value = DEFAULT_Q_VALUE;
    char q_value_s[ENCODING_STR_MAX_SIZE];
    char encodingStr[ENCODING_STR_MAX_SIZE];

    int count = sscanf(qvString, " %63[^;];q=%63s", encodingStr, q_value_s);
    if (count == 0)
    {
        return NULL;
    }
    if (count > 1)
    {
        q_value = strtof(q_value_s, NULL);
    }
    EncodingQualityValue *encoding = custom_alloc(sizeof(EncodingQualityValue));
    encoding->encoding = HTTP_ENCODING(encodingStr);
    encoding->q = q_value;
    return (void *)encoding;
}

void setTransferEncoding(HTTPResponse *response, enum http_encoding encoding)
{
    response->transferEncoding = encoding;
}

int decideContentEncoding(CC_PQueue *encqueue, enum http_encoding *chosenEncoding)
{
    EncodingQualityValue *topChoice;
    enum cc_stat stat = cc_pqueue_pop(encqueue, (void **)&topChoice);
    if (stat != CC_OK)
    {
        goto error;
    }

    switch (topChoice->encoding)
    {
    case WILDCARD:
        *chosenEncoding = DEFAULT_ENCODING;
        return 0;
    case UNKNOWN_ENCODING:
        // chunked is only for TE Headers
        goto error;
    case CHUNKED:
        goto error;
    default:
        *chosenEncoding = topChoice->encoding;
        return 0;
    }
    return 0;
error:
    *chosenEncoding = UNKNOWN_ENCODING;
    return -1;
}

int cmpEncodingQV(const void *ptr1, const void *ptr2)
{
    EncodingQualityValue *qv1 = (EncodingQualityValue *)ptr1;
    EncodingQualityValue *qv2 = (EncodingQualityValue *)ptr2;
    if (qv1->q > qv2->q)
    {
        return 1;
    }
    if (qv1->q < qv2->q)
    {
        return -1;
    }
    return 0;
}

CC_PQueue *decodeAcceptEncodings(CC_Deque *teList)
{
    return decodeQualityValues(teList, decodeSingleEncodingQualityValue, cmpEncodingQV);
}

int decideTransferEncoding(CC_PQueue *encqueue, enum http_encoding *chosenEncoding)
{
    EncodingQualityValue *topChoice;
    enum cc_stat stat = cc_pqueue_pop(encqueue, (void **)&topChoice);
    if (stat != CC_OK)
    {
        goto error;
    }

    switch (topChoice->encoding)
    {
    case WILDCARD:
        *chosenEncoding = DEFAULT_ENCODING;
        return 0;
    case UNKNOWN_ENCODING:
        goto error;
    default:
        *chosenEncoding = topChoice->encoding;
        return 0;
    }
    return 0;
error:
    *chosenEncoding = UNKNOWN_ENCODING;
    return -1;
}

void addTransferEncodingToQueue(char *inStr, void *args)
{

    CC_Queue *queue = args;
    char trimmedStr[strlen(inStr)];
    int count = sscanf(inStr," %s ",trimmedStr);
    assert(count == 1);
    enum http_encoding *ptr = custom_alloc(sizeof(enum http_encoding));
    if (ptr && *ptr != UNKNOWN_ENCODING)
    {
        // error
        *ptr = HTTP_ENCODING(inStr);
        cc_queue_enqueue(queue, ptr);
    }
}

void decodeTransferCodingString(char *qvString, CC_Queue **queue)
{
    CC_QueueConf conf;
    cc_queue_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;

    cc_queue_new_conf(&conf, queue);

    tokenize(qvString, ',', addTransferEncodingToQueue, (void*)*queue);
}
