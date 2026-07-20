#pragma once

#include "cc_deque.h"
#include "cc_hashtable.h"
#include "memory/cc_dynamic_pool.h"
#include <asm-generic/errno-base.h>
#include <utils.h>

#define CONTENT_LENGTH_HEADER_NAME "Content-Length"
#define TRANSFER_CODING_HEADER_NAME "Transfer-Encoding"
#define TRANSFER_ENCODING_CLIENT_HEADER_NAME "TE"
#define CONTENT_ENCODING_HEADER_NAME "Content-Encoding"
#define ACCEPT_ENCODING_HEADER_NAME "Accept-Encoding"
#define CONTENT_TYPE_HEADER_NAME "Content-Type"
#define HOST_HEADER_NAME "Host"
#define LOCATION_HEADER_NAME "Location"
#define CONNECTION_HEADER_NAME "Connection"

// method
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_DELETE "DELETE"
#define HTTP_METHOD_HEAD "HEAD"
#define HTTP_METHOD_OPTIONS "OPTIONS"
#define HTTP_NUM_SUPPORTED_METHODS 6

enum http_method {GET=0, POST=1, PUT=2, DELETE=3, HEAD=4, OPTIONS=5};

extern const char* http_method_arr[HTTP_NUM_SUPPORTED_METHODS] ;

#define HTTP_NUM_SUPPORTED_ENCODINGS 3

enum http_encoding {GZIP=0, DEFLATE=1, IDENTITY=2};


#define GZIP_HEADER_VALUE "gzip"
#define ZLIB_HEADER_VALUE "deflate"
#define IDENTITY_HEADER_VALUE "identity"

extern const char* http_encoding_arr[HTTP_NUM_SUPPORTED_ENCODINGS] ;

#define MULTIPART_FORMDATA_VALUE "multipart/form-data"
#define TEXT_HTML_VALUE "text/html"
#define TEXT_PLAIN_VALUE "text/plain"

#define HTTP_NUM_SUPPORTED_CONTENT_TYPE 3

enum http_content_type { multipart_formdata=0, text_html=1, text_plain=2 };

extern const char* http_content_type_arr[HTTP_NUM_SUPPORTED_CONTENT_TYPE] ;

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
    TRAILER_CHUNK_REACEHED,
    EMPTY_LINE,
    RECV_ERROR,
    RECV_SUCCESS,
    BUF_TOO_BIG,
};

#define HTTP_STREAM_MAX_BUFFER 4096
#define HTTP_STREAM_INIT_BUFFER 64
#define HTTP_POOL_INITIAL_SIZE 2<<16
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
  Path uriPath;
  char* uri;
  enum http_method method;
  char* version;
  int contentLength;
  char *body;
  enum http_encoding contentEncoding; // done by client
  enum http_encoding transferEncoding; // done hop by hop
  CC_DynamicPool* pool;

} HTTPRequest;

typedef struct {
  HTTPRequest *request;
  CC_HashTable * headers;
  int contentLength;
  int statusCode;
  char *body;
  char* version;
  CC_DynamicPool* pool;
  enum http_encoding contentEncoding;
  enum http_encoding transferEncoding;
  enum http_content_type contentType;

} HTTPResponse;

int initHTTPRequest(HTTPRequest *request, CC_DynamicPool* pool);
int initHTTPResponse(HTTPResponse *response, CC_DynamicPool* pool);
void freeHTTPRequest(HTTPRequest *request);
void freeHTTPResponse(HTTPResponse *response);
void setRequest(HTTPResponse *resp, HTTPRequest *resq);
char* getHeader(CC_HashTable *dict, char *key);
CC_Deque* getHeaderValues(CC_HashTable *dict, char *key);
void setHeaderPool(CC_HashTable* dict, CC_DynamicPool* pool,char *key, char *value);
#define setHeader(r,k,v) setHeaderPool((r)->headers,(r)->pool, k, v )
#define setQueryParam(r,k,v) setHeaderPool((r)->urlParams,(r)->pool, k, v )

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength) ;
void init_code_to_phrase();
