#include "cc_array.h"
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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <zlib.h>
#include <zstd.h>

thread_local magic_t magic;


#define ENCODING_STR_MAX_SIZE 64
#define MIMETYPE_STR_MAX_SIZE 64
#define DEFAULT_Q_VALUE 1.0f
#define DEFAULT_MIMETYPE_STR "application/octet-stream"


bool matchMimeType(MimeTypeQualityValue *acceptQv, MimeTypeQualityValue *actualQv)
{
    if (!strcmp(acceptQv->major, "*"))
    {
        return true;
    }
    if (strcmp(acceptQv->major, actualQv->major) != 0)
    {
        return false;
    }
    if (!strcmp(acceptQv->minor, "*")){
        return true;
    }
    return strcmp(acceptQv->minor, actualQv->minor) == 0;
}



int cmpMimeTypeSpecificity(MimeTypeQualityValue *qv1, MimeTypeQualityValue *qv2)
{
    int majorGen1 = strcmp(qv1->major, "*");
    int majorGen2 = strcmp(qv2->major, "*");
    if (majorGen1 && !majorGen2)
    {
        return 1;
    }
    if (!majorGen1 && majorGen2)
    {
        return -1;
    }
    // only compare if major types match
    // if one has the minor type specfied and one doesnt
    // choose the the more specific one
    int minGen1 = strcmp(qv1->minor, "*");
    int minGen2 = strcmp(qv2->minor, "*");
    if (minGen1 && !minGen2)
    {
        return 1;
    }
    if (!minGen1 && minGen2)
    {
        return -1;
    }
    return 0;
}

bool mimeTypeContains(MimeTypeQualityValue* choice, MimeTypeQualityValue* actual){
    int majorGen1 = strcmp(choice->major, "*");
    if (!majorGen1 )
    {
        return true;
    }

    if (strcmp(choice->major, actual->major)!=0){
        return false;
    }
    // only compare if major types match
    // if one has the minor type specfied and one doesnt
    // choose the the more specific one
    int minGen1 = strcmp(choice->minor, "*");
    if (!minGen1 )
    {
        return true;
    }

    return (strcmp(choice->minor, actual->minor)== 0);
}

void *decodeSingleEncodingQualityValue(char *qvString)
{
    float q_value = DEFAULT_Q_VALUE;
    char q_value_s[ENCODING_STR_MAX_SIZE];
    char *q_value_s_endptr = NULL;
    char encodingStr[ENCODING_STR_MAX_SIZE];

    int count = sscanf(qvString, "%63[^;];q=%63s", encodingStr, q_value_s);
    if (count == 0)
    {
        return NULL;
    }
    if (count > 1)
    {
        q_value = strtof(q_value_s, &q_value_s_endptr);
    }
    EncodingQualityValue *encoding = custom_alloc(sizeof(EncodingQualityValue));
    encoding->encoding = HTTP_ENCODING(encodingStr);
    encoding->q = q_value;
    return (void *)encoding;
}

void *decodeSingleMimetypeQualityValue(char *qvString)
{
    float q_value = 1.0f;
    char valueMajor[64];
    char valueMinor[64];
    int count = sscanf(qvString, " %63[^/]/%63[^;];q=%f", valueMajor, valueMinor, &q_value);
    if (count <= 1)
    {
        return NULL;
    }
    MimeTypeQualityValue *ptr = custom_alloc(sizeof(MimeTypeQualityValue));
    ptr->major = custom_strdup(valueMajor);
    ptr->minor = custom_strdup(valueMinor);
    ptr->q = q_value;
    return (void *)ptr;
}

void decodeQualityValueString(char *qvString, CC_PQueue *pqueue, void *(*decodeFunc)(char *))
{
    int strLen = strlen(qvString);
    char currentString[strLen];

    int index = 0;
    int consumedIndex = 0;
    int writeIndex = 0;
    while (index < strLen+1)
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

int cmpMimeTypeQV(const void *ptr1, const void *ptr2)
{
    MimeTypeQualityValue *qv1 = (MimeTypeQualityValue *)ptr1;
    MimeTypeQualityValue *qv2 = (MimeTypeQualityValue *)ptr2;
    if (qv1->q > qv2->q)
    {
        return 1;
    }
    if (qv1->q < qv2->q)
    {
        return -1;
    }
    return cmpMimeTypeSpecificity(qv1, qv2);
}

int cmpEncodingQV(const void *ptr1, const void *ptr2)
{
    EncodingQualityValue *qv1 = (EncodingQualityValue *)ptr1;
    EncodingQualityValue *qv2 = (EncodingQualityValue *)ptr2;
    if (qv1->q > qv2->q)
    {
        return -1;
    }
    if (qv1->q < qv2->q)
    {
        return 1;
    }
    return 0;
}

CC_PQueue *decodeAcceptEncodings(CC_Deque *teList)
{
    return decodeQualityValues(teList, decodeSingleEncodingQualityValue, cmpEncodingQV);
}
CC_PQueue *decodeAcceptTypes(CC_Deque *teList)
{
    return decodeQualityValues(teList, decodeSingleMimetypeQualityValue, cmpMimeTypeQV);
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

enum http_encoding decideEncoding(CC_Deque *teList, CC_Array *supportedEncodings)
{
    return IDENTITY;
}
void setTransferEncoding(HTTPResponse *response, enum http_encoding encoding)
{
    response->transferEncoding = encoding;
}


int decideContentType(CC_PQueue *ctqueue, char *actualMimetype, char **decidedMimetype)
{
    MimeTypeQualityValue *topChoice;
    MimeTypeQualityValue *actual = decodeSingleMimetypeQualityValue(actualMimetype);
    enum cc_stat stat;
    while ((stat = cc_pqueue_pop(ctqueue, (void **)&topChoice)) == CC_OK)
    {
        if (matchMimeType(topChoice, actual))
        {
            *decidedMimetype = actualMimetype;
            return 0;
        }
        // if result are not the same
    }
    *decidedMimetype = NULL;
    return -1;
}
void setContentType(HTTPResponse *response, char *mimetype)
{
    response->contentType = mimetype;
}

char *getMimeTypeForFile(char *filepath)
{
    loadMagicDB();
    const char *mime = magic_file(magic, filepath);
    if (!mime)
    {
        return custom_strdup(DEFAULT_MIMETYPE_STR);
    }
    return custom_strdup((char *)mime);
}

int loadMagicDB()
{
    magic = magic_open(MAGIC_MIME);
    if (magic_load(magic, NULL) != 0)

    {
        const char * exp = magic_error(magic);
        fprintf(stderr, "%s",exp);
        magic_close(magic);
        return -1;
    }
    return 0;
}


void encode_gzip(char *data)
{
}
