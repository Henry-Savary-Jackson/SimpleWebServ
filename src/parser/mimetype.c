#include "server.h"
#include "utils.h"
#include <ctype.h>
#include <http.h>
#include <linux/limits.h>
#include <magic.h>
#include <netinet/in.h>
#include <parser.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <zconf.h>
#include <zlib.h>
#include <zstd.h>


thread_local magic_t magic;

#define DEFAULT_MIMETYPE_STR "application/octet-stream"

void decodeMediaTypeParam(char* inStr, void* args){
    MediaType * mediaType = args;
    char name[1<<6];
    char value[1<<6];
    int countOpts = sscanf(inStr, " %63[^=]=%63s ", name, value  );
    if (countOpts != 2){
        return ;
    }
    char * nameLower;
    char * valueLower;
    strToLower(name, &nameLower);
    strToLower(value, &valueLower);

    if (!strcmp( nameLower, "charset")){
        mediaType->charset = valueLower;
    }else if (!strcmp(valueLower, "boundary" )){
        mediaType->boundary = valueLower;
    }
}

int decodeRequestContentMimeType(char * qvString, MediaType* mediaType){
    char valueMajor[1 << 6];
    char valueMinor[1 << 6];
    char opts[1 << 8];

    mediaType->boundary = NULL;
    mediaType->charset = NULL;

    int count = sscanf(qvString, " %63[^/]/%63[^;]; %255s ", valueMajor, valueMinor, opts);
    if (count < 2 ){
        return -1;
    }
    strToLower(valueMajor, &mediaType->major);
    strToLower(valueMinor, &mediaType->min);

    if (count == 3){
        tokenize(opts, ';', decodeMediaTypeParam,(void*) mediaType);
    }
    return 0;
}

void *decodeSingleMimetypeQualityValue(char *qvString)
{
    float q_value = 1.0f;
    char valueMajor[1 << 6];
    char valueMinor[1 << 6];
    int count = sscanf(qvString, " %63[^/]/%63[^;];q=%f", valueMajor, valueMinor, &q_value);
    if (count <= 1)
    {
        return NULL;
    }
    MimeTypeQualityValue *ptr = custom_alloc(sizeof(MimeTypeQualityValue));
    strToLower(valueMajor, &ptr->major);
    strToLower(valueMinor, &ptr->minor);
    ptr->q = q_value;
    return (void *)ptr;
}



void setContentType(HTTPResponse *response, char *mimetype)
{
    response->contentType = mimetype;
}

char *getMimeTypeForFile(char *filepath)
{
    // Require that load MagicDB called beforehand
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
        const char *exp = magic_error(magic);
        fprintf(stderr, "%s", exp);
        magic_close(magic);
        return -1;
    }
    return 0;
}
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
    if (!strcmp(acceptQv->minor, "*"))
    {
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

CC_PQueue *decodeAcceptTypes(CC_Deque *teList)
{
    return decodeQualityValues(teList, decodeSingleMimetypeQualityValue, cmpMimeTypeQV);
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
