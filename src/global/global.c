#include "global.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void initString(String* s){
    bzero(s, sizeof(String));
}

void freeString(String* s){
    if (s->buf)
        free(s->buf);
}

void copyString(String* dst, char * src){
    freeString(dst);
    dst->length = strlen(src);
    dst->buf = malloc(dst->length);
    strcpy(dst->buf, src);
}