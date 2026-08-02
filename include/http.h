#pragma once

#include "cc_deque.h"
#include "cc_hashtable.h"
#include <asm-generic/errno-base.h>
#include <zlib.h>
#include "utils.h"

#define CONTENT_LENGTH_HEADER_NAME "Content-Length"
#define TRANSFER_CODING_HEADER_NAME "Transfer-Encoding"
#define TRANSFER_ENCODING_CLIENT_HEADER_NAME "TE"
#define CONTENT_ENCODING_HEADER_NAME "Content-Encoding"
#define ACCEPT_MIMETYPE_HEADER_NAME "Accept"
#define ACCEPT_ENCODING_HEADER_NAME "Accept-Encoding"
#define CONTENT_TYPE_HEADER_NAME "Content-Type"
#define HOST_HEADER_NAME "Host"
#define LOCATION_HEADER_NAME "Location"
#define CONNECTION_HEADER_NAME "Connection"
#define COOKIE_CLIENT_HEADER_NAME "Cookie"
#define COOKIE_SERVER_HEADER_NAME "Set-Cookie"

// method
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_DELETE "DELETE"
#define HTTP_METHOD_HEAD "HEAD"
#define HTTP_METHOD_OPTIONS "OPTIONS"
#define METHOD_UNKNOWN_STR "Unknown"
#define HTTP_NUM_SUPPORTED_METHODS 6

enum http_method {GET=0, POST=1, PUT=2, DELETE=3, HEAD=4, OPTIONS=5, UNKNOWN_METHOD=6};

extern const char* http_method_arr[HTTP_NUM_SUPPORTED_METHODS+1] ;

#define HTTP_NUM_SUPPORTED_ENCODINGS 4

enum http_encoding {GZIP=0, DEFLATE=1, IDENTITY_ENCODING=2,CHUNKED=3,  WILDCARD=4 ,UNKNOWN_ENCODING=5};

#define DEFAULT_ENCODING IDENTITY_ENCODING
#define DEFAULT_TRANSFER_CODING UNKNOWN_ENCODING

#define GZIP_HEADER_VALUE "gzip"
#define ZLIB_HEADER_VALUE "deflate"
#define IDENTITY_HEADER_VALUE "identity"
#define CHUNKED_HEADER_VALUE "chunked"
#define WILDCARD_HEADER_VALUE "*"
#define ENCODING_UNKNOWN_STR "Unknown"

extern const char* http_encoding_arr[HTTP_NUM_SUPPORTED_ENCODINGS+2] ;

#define MULTIPART_FORMDATA_VALUE "multipart/form-data"
#define TEXT_HTML_VALUE "text/html"
#define TEXT_PLAIN_VALUE "text/plain"

#define HTTP_NUM_SUPPORTED_CONTENT_TYPE 3

enum http_content_type { multipart_formdata=0, text_html=1, text_plain=2 };

extern const char* http_content_type_arr[HTTP_NUM_SUPPORTED_CONTENT_TYPE+1] ;

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
extern const int HTTP_NOT_ACCEPTED;
extern const int HTTP_CONTENT_LENGTH_REQUIRED;
extern const int HTTP_UNSUPPORTED_MEDIA_TYPE ;
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
extern const char *HTTP_NOT_ACCEPTED_PHRASE;
extern const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE;
extern const char * HTTP_UNSUPPORTED_MEDIA_TYPE_PHRASE ;
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

#define HTTP_STREAM_MAX_BUFFER 2<<24
#define HTTP_STREAM_INIT_BUFFER 2<<10
#define HTTP_POOL_INITIAL_SIZE 2<<16
extern const char *HTTP_LINE_END_TOK;
extern const int HTTP_LINE_END_TOK_SIZE;


typedef struct
{
    z_stream zstrm;
    int capacity;
    int connfd;
    int head;
    int consumedoffset;
    int writeoffset;
    int tokenIndex; // how far into the end token has been reached, incase stream terminates at CR char
    char *ptr;
} HTTPStream;

void initHTTPStream(HTTPStream *stream, int connfd);
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
    char * major;
    char * min;
    char* charset;
   char*  boundary;
} MediaType;

typedef struct {
  CC_HashTable * headers;
  CC_HashTable *urlParams;
  CC_HashTable *cookies;
  Path uriPath;
  char* uri;
  enum http_method method;
  char* version;
  int contentLength;
  char *body;
  enum http_encoding contentEncoding; // done by client
  enum http_encoding transferEncoding; // done hop by hop
  MediaType contentType;
  HTTPStream* inputStream;
} HTTPRequest;

typedef struct {
  HTTPRequest *request;
  CC_HashTable * headers;
  CC_HashTable *cookies;
  int contentLength;
  int statusCode;
  char *body;
  char* version;
  enum http_encoding contentEncoding;
  enum http_encoding transferEncoding;
  char * contentType;

} HTTPResponse;

int initHTTPRequest(HTTPRequest *request);
int initHTTPResponse(HTTPResponse *response);
void freeHTTPRequest(HTTPRequest *request);
void freeHTTPResponse(HTTPResponse *response);


void configureHTTPDict(CC_HashTableConf* conf);
void configureHTTPDictGlobal(CC_HashTableConf *conf);
void setRequest(HTTPResponse *resp, HTTPRequest *resq);
char* getValueDict(CC_HashTable *dict, char *key);
CC_Deque* getValuesDict(CC_HashTable *dict, char *key);
void putKVDict(CC_HashTable* dict,char *key, char *value);

#define setHeader(r,k,v) putKVDict((r)->headers, k, v )
#define getHeader(r,k) getValueDict((r)->headers, k )
#define getHeaderValues(r,k) getValuesDict((r)->headers, k )

#define setQueryParam(r,k,v) putKVDict((r)->urlParams, k, v )
#define getQueryParam(r,k) getValueDict((r)->urlParams, k )
#define getQueryParamValues(r,k) getValuesDict((r)->urlParams, k )

#define setCookieRequest(r,k,v) putKVDict((r)->cookies, k, v )
#define getCookieRequest(r,k) getValueDict((r)->cookies, k )
#define getCookieValuesRequest(r,k) getValuesDict((r)->cookies, k )


void setResponseBody(HTTPResponse *response, char *buffer, int contentLength) ;
void init_code_to_phrase();

int makeSuccessEmpty(HTTPResponse* response);
int makeErrorResponse(HTTPResponse* response, int status, char * message);
int makeBadRequest(HTTPResponse* response, char * reason); //
int makeUnauthorized(HTTPResponse* response, char * reason);
int makeForbidden(HTTPResponse* response, char * reason);
int makeNotFound(HTTPResponse* response, char * reason);
int makeContentLengthRequired(HTTPResponse* response, char * reason);
int makeMethodNotSupported(HTTPResponse* response, char * reason);
int makeNotAccepable(HTTPResponse* response, char * reason);
int makeMediaTypeNotSupported(HTTPResponse* response, char * reason);
int makeServerErrror(HTTPResponse* response, char * reason);
