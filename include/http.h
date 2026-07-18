#pragma once

#include "cc_hashtable.h"
#include "memory/cc_dynamic_pool.h"
#include <utils.h>

#define CONTENT_LENGTH_HEADER_NAME "Content-Length"
#define TRANSFER_CODING_HEADER_NAME "Transfer-Coding"
#define HOST_HEADER_NAME "Host"
#define LOCATION_HEADER_NAME "Location"
#define CONNECTION_HEADER_NAME "Connection"

// method
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_DELETE "DELETE"

// status code
extern const int HTTP_OK ;
extern const int HTTP_NO_CONTENT ;
extern const int HTTP_MOVED ;
extern const int HTTP_NOT_MODIFIED ;
extern const int HTTP_BAD_REQUEST ;
extern const int HTTP_UNAUTHORIZED;
extern const int HTTP_FORBIDDEN;
extern const int HTTP_NOT_FOUND;
extern const int HTTP_METHOD_UNSUPPORTED;
extern const int HTTP_CONTENT_LENGTH_REQUIRED;
extern const int HTTP_SERVER_ERROR;

// status code reason phrase

extern const char *HTTP_OK_PHRASE;
extern const char *HTTP_NO_CONTENT_PHRASE;
extern const char *HTTP_MOVED_PHRASE;

extern const char *HTTP_NOT_MODIFIED_PHRASE;
extern const char *HTTP_BAD_REQUEST_PHRASE;
extern const char *HTTP_UNAUTHORIZED_PHRASE;
extern const char *HTTP_FORBIDDEN_PHRASE;
extern const char *HTTP_NOT_FOUND_PHRASE;
extern const char *HTTP_METHOD_UNSUPPORTED_PHRASE;
extern const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE;
extern const char *HTTP_SERVER_ERROR_PHRASE;

typedef struct {
  int key;
  const char *value;
} HttpCodePhrase;

extern CC_HashTable *code_to_phrase;

enum http_stream_status
{
    LF_REACHED,
    EOF_REACHED,
    EMPTY_LINE,
    RECV_ERROR,
    RECV_SUCCESS,
    BUF_TOO_BIG,
};

#define HTTP_STREAM_MAX_BUFFER 4096
#define HTTP_STREAM_INIT_BUFFER 64
extern const char *HTTP_LINE_END_TOK;
extern const int HTTP_LINE_END_TOK_SIZE;

typedef struct
{
    int capacity;
    int connfd;
    int head;
    int consumedoffset;
    int writeoffset;
    int tokenIndex; // how far into the end token has been reached, incase stream terminates at CR char
    char *ptr;
    CC_DynamicPool *pool;
} HTTPStream;

void initHTTPStream(HTTPStream *stream, int connfd, CC_DynamicPool *pool);
enum http_stream_status consumeUntilLineFeed(HTTPStream *stream);
enum http_stream_status readLine(HTTPStream *stream, int *lineLength, char **output);
enum http_stream_status consumeBody(HTTPStream *stream, char **out, int contentLength);
void consumeUntilEOF(HTTPStream *stream, char *dest, int *nWritten);
enum http_stream_status receiveData(HTTPStream *stream);


typedef struct {
  char *key;
  char *value;
} Header;

typedef struct {
  CC_HashTable * headers;
  CC_HashTable *urlParams;
  char* uri;
  char* method;
  char* version;
  int contentLength;
  char *body;
  CC_DynamicPool* pool;

} HTTPRequest;

typedef struct {
  HTTPRequest *request;
  CC_HashTable * headers;
  int contentLength;
  int statusCode;
  char *body;
  CC_DynamicPool* pool;

} HTTPResponse;

int initHTTPRequest(HTTPRequest *request);
int initHTTPResponse(HTTPResponse *response);
void freeHTTPRequest(HTTPRequest *request);
void freeHTTPResponse(HTTPResponse *response);
void setRequest(HTTPResponse *resp, HTTPRequest *resq);
void allocDictToArena(CC_HashTable* dict, char *key, char *value);
#define setHeader(r,k,v) allocDictToArena((r)->headers, k, v )
#define setQueryParam(r,k,v) allocDictToArena((r)->urlParams, k, v )

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength) ;
void init_code_to_phrase();
