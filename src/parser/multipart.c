



#include "cc_hashtable.h"
#include "http.h"
#include "parser.h"
#include "utils.h"
#include <assert.h>
#include <uuid/uuid.h>

int makeBoundary(const char outputBoudary[UUID_STR_LEN]){
    uuid_t uuidbin;
    uuid_generate_random(uuidbin);
    uuid_parse(outputBoudary,uuidbin);
    return 0;
}

int appendMultiPartForm(char * buffer, size_t size ,char* boundary,CC_HashTable* headers , bool isEOF, GrowingBuffer* outBuffer){

    size_t lenBoundary=strlen(boundary);
    appendGrowingBuffer(outBuffer, boundary, lenBoundary);
    appendGrowingBuffer(outBuffer, (char*)HTTP_LINE_END_TOK, HTTP_LINE_END_TOK_SIZE);
    encodeHeaders(headers, outBuffer );
    char tempBuf[size+lenBoundary +  HTTP_LINE_END_TOK_SIZE +1];
    // data + newLine + boundary + "--" string + null char
    int count = snprintf(tempBuf, sizeof(tempBuf),"%s%s%s",buffer, HTTP_LINE_END_TOK, boundary  );
    if (count <= sizeof(tempBuf)){
        return -1;
    }
    appendGrowingBuffer(outBuffer, tempBuf, strlen(tempBuf));
    if (isEOF){
        appendGrowingBuffer(outBuffer, "--", 2);
        return 0;
    }
    appendGrowingBuffer(outBuffer, (char*)HTTP_LINE_END_TOK, HTTP_LINE_END_TOK_SIZE);

    return 0;
}
