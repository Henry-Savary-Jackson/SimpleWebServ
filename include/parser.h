#pragma once

#include "cc_array.h"
#include "cc_deque.h"
#include "cc_hashtable.h"
#include "cc_pqueue.h"
#include "http.h"
#include "utils.h"
#include <magic.h>

typedef struct {
    char * major;
    char * minor;
    float q;
} MimeTypeQualityValue;

typedef struct {
    char * major;
    char * min;
    char* charset;
   char*  boundary;
} MediaType;

typedef struct {
    enum http_encoding encoding;
    float q;
} EncodingQualityValue ;

extern thread_local magic_t magic;

int scanRequest(int connfd, HTTPRequest *request, bool *keepAliv, int* status) ;

int sendResponse(HTTPResponse *response, int connfd) ;
int encodeHeaders(CC_HashTable* headers, GrowingBuffer* buffer) ;
int prepareHTTPResponseMetadata(HTTPResponse* response, GrowingBuffer* buffer);
int prepareResponseBody(HTTPResponse* response, GrowingBuffer* outBuffer);


/* Chunked body reading
*/
int readBodyChunked(HTTPStream* stream, HTTPRequest* request);
enum http_stream_status readNextChunk(HTTPStream* stream, char** out,int* size);
enum http_stream_status readTrailerSection(HTTPStream* stream, CC_HashTable* trailerParams);

/** send body chunks
 */
int sendChunk(int connfd, char* chunk, int size);
int sendFinalChunk(int connfd);
int sendTrailerChunk(int connfd, CC_HashTable* trailerParams);
int sendBodyChunks(int connfd, HTTPResponse* response, int maxChunkSize );

/*
 *
 */

CC_PQueue* decodeQualityValues(CC_Deque* teList, void* (*decodeFunc)(char*), int (*cmp)(const void*, const void*));
CC_PQueue* decodeAcceptEncodings(CC_Deque* teList);
CC_PQueue* decodeAcceptTypes(CC_Deque* teList);
void *decodeSingleMimetypeQualityValue(char *qvString);
void *decodeSingleEncodingQualityValue(char *qvString);

enum http_encoding decideEncoding(CC_Deque* teList, CC_Array* supportedEncodings);
void setTransferEncoding(HTTPResponse* response, enum http_encoding encoding);

int decideContentType(CC_PQueue* ctqueue, char* actualMimetype, char** decidedMimetype);
void setContentType(HTTPResponse* response, char* mimetype);

char* getMimeTypeForFile(char* filepath);

int loadMagicDB();
