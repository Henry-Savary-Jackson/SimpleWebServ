#include "cc_common.h"
#include "cc_pqueue.h"
#include "server.h"
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
    enum cc_stat stat;
    while ((stat = cc_pqueue_pop(encqueue, (void **)&topChoice)) == CC_OK)
    {
        // if the encoding string is unknown
        if (topChoice->encoding == WILDCARD)
        {
            return DEFAULT_ENCODING;
        }
        if (topChoice->encoding != UNKNOWN_ENCODING)
        {
            return topChoice->encoding;
        }
    }
    *chosenEncoding = UNKNOWN_ENCODING;
    return -1;
    return 0;
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
