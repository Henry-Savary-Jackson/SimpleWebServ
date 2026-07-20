#pragma once


#include "cc_array.h"
#include "cc_deque.h"
#include "cc_hashtable.h"
#include "cc_pqueue.h"
#include "http.h"
#include "utils.h"
int scanRequest(int connfd, HTTPRequest *request, bool *keepAliv, int* status) ;

int sendResponse(HTTPResponse *response, int connfd) ;
int encodeHeaders(CC_HashTable* headers, GrowingBuffer* buffer) ;

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

void decodeQualityValueString(char * teString, CC_PQueue* pq);
CC_PQueue* decodeQualityValues(CC_Deque* teList);
enum http_encoding decideEncoding(CC_Deque* teList, CC_Array* supportedEncodings);
void setTransferEncoding(HTTPResponse* response, enum http_encoding encoding);
