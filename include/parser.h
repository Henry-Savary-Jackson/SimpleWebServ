#pragma once

#include "cc_deque.h"
#include "cc_hashtable.h"
#include "cc_pqueue.h"
#include "http.h"
#include "utils.h"
#include <magic.h>
#include <zlib.h>

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
int encodeResponseBody(HTTPResponse* response, GrowingBuffer* buffer) ;
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
#define ENCODING_STR_MAX_SIZE 64
#define MIMETYPE_STR_MAX_SIZE 64
#define DEFAULT_Q_VALUE 1.0f

CC_PQueue* decodeQualityValues(CC_Deque* teList, void* (*decodeFunc)(char*), int (*cmp)(const void*, const void*));
CC_PQueue* decodeAcceptEncodings(CC_Deque* teList);
CC_PQueue* decodeAcceptTypes(CC_Deque* teList);
void *decodeSingleMimetypeQualityValue(char *qvString);
void *decodeSingleEncodingQualityValue(char *qvString);

int decideContentEncoding(CC_PQueue *encqueue, enum http_encoding* chosenEncoding);
void setTransferEncoding(HTTPResponse* response, enum http_encoding encoding);

int decideContentType(CC_PQueue* ctqueue, char* actualMimetype, char** decidedMimetype);
void setContentType(HTTPResponse* response, char* mimetype);

char* getMimeTypeForFile(char* filepath);

int loadMagicDB();

int decode_zlib_prepare(z_streamp strm);
int encode_zlib_prepare(z_streamp strm);
int encode_gzip_prepare(z_streamp strm);
int decode_gzip_prepare(z_streamp strm);

int decode_zstream(z_streamp strm, char **output, int *outSize);
int encode_zlib(char *data, int inSize, char **output, int *outSize);
int decode_zlib(char *data, int inSize, char **output, int *outSize);

int encode_gzip(char *data, int inSize, char **output, int *outSize);
int decode_gzip(char *data, int inSize, char **output, int *outSize);

int compressChunk(z_streamp strm, char *chunk, int chunkSize, char *outChunk);
int inflateChunk(z_streamp strm, int outSize, char *outChunk);
