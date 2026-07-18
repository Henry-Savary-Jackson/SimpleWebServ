#include "cc_common.h"
#include "cc_hashtable.h"
#include "memory/cc_dynamic_pool.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils.h>


const int HTTP_OK = 200;
const int HTTP_NO_CONTENT = 204;
const int HTTP_MOVED = 301;
const int HTTP_NOT_MODIFIED = 304;
const int HTTP_BAD_REQUEST = 400;
const int HTTP_UNAUTHORIZED = 401;
const int HTTP_FORBIDDEN = 403;
const int HTTP_NOT_FOUND = 404;
const int HTTP_METHOD_UNSUPPORTED = 405;
const int HTTP_CONTENT_LENGTH_REQUIRED = 411;
const int HTTP_SERVER_ERROR = 500;

const char *HTTP_OK_PHRASE = "OK";
const char *HTTP_NO_CONTENT_PHRASE = "No Content";
const char *HTTP_MOVED_PHRASE = "Moved Permanently";
const char *HTTP_NOT_MODIFIED_PHRASE = "Not Modified";
const char *HTTP_BAD_REQUEST_PHRASE = "Bad Request";
const char *HTTP_UNAUTHORIZED_PHRASE = "Unauthorized";
const char *HTTP_FORBIDDEN_PHRASE = "Forbidden";
const char *HTTP_NOT_FOUND_PHRASE = "Not Found";
const char *HTTP_METHOD_UNSUPPORTED_PHRASE = "Method Not Allowed";
const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE = "Length Required";
const char *HTTP_SERVER_ERROR_PHRASE = "Internal Server Error";

CC_HashTable *code_to_phrase;
const char *HTTP_LINE_END_TOK = "\r\n";
const int HTTP_LINE_END_TOK_SIZE = strlen("\r\n");

#define HTTP_REQUEST_INIT 4096

void initHTTPStream(HTTPStream *stream, int connfd, CC_DynamicPool *pool)
{
    stream->capacity = HTTP_STREAM_INIT_BUFFER;
    stream->connfd = connfd;
    stream->tokenIndex = 0;
    stream->ptr = cc_dynamic_pool_malloc(stream->capacity, stream->pool);
    stream->writeoffset = 0;
    stream->consumedoffset = 0;
    stream->head = 0;
    stream->pool = pool;
}
enum http_stream_status consumeUntilLineFeed(HTTPStream *stream)
{

    bool cr = false;
    while (stream->head < stream->writeoffset)
    {
        char c = stream->ptr[stream->head];
        stream->head++;

        stream->tokenIndex = (c == HTTP_LINE_END_TOK[stream->tokenIndex]) ? stream->tokenIndex + 1 : 0;
        if (stream->tokenIndex >= HTTP_LINE_END_TOK_SIZE)
        {
            // set to be null terminated, so that sscanf and strlen works
            stream->ptr[stream->head - HTTP_LINE_END_TOK_SIZE] = 0;
            return LF_REACHED;
        }

        if (cr)
            stream->ptr[stream->head - HTTP_LINE_END_TOK_SIZE] = ' ';
        // replace lone CR with space according to RFC 9112

        cr = !cr && c == '\r'; // detect lone carriage return
    }
    return EOF_REACHED;
}
enum http_stream_status readLine(HTTPStream *stream, int *lineLength, char **output)
{
    enum http_stream_status lineReadStatus;
    while ((lineReadStatus = consumeUntilLineFeed(stream)) != LF_REACHED)
    {
        // receive more data until a line feed is reached
        enum http_stream_status recvStat = receiveData(stream);
        if (recvStat != RECV_SUCCESS)
        {
            return recvStat;
        }
    }
    *lineLength = stream->writeoffset - stream->consumedoffset - HTTP_LINE_END_TOK_SIZE;
    *output = stream->ptr + stream->consumedoffset;
    stream->consumedoffset = stream->head;
    return lineLength >= 0 ? LF_REACHED : EMPTY_LINE;
}

void consumeUntilEOF(HTTPStream *stream, char *dest, int *nWritten)
{
    *nWritten = stream->writeoffset - stream->consumedoffset;
    assert(nWritten >= 0);
    memcpy(dest, stream->ptr + stream->consumedoffset, *nWritten);
    stream->consumedoffset = stream->writeoffset;
}

enum http_stream_status consumeBody(HTTPStream *stream, char **out, int contentLength)
{

    enum http_stream_status status;
    while (stream->writeoffset - stream->consumedoffset < contentLength &&
           ((status = receiveData(stream)) == RECV_SUCCESS))
    {
        // if the error code is simply the handling of a signal, then just
        // continue
        if (status == ERROR_RECV)
        {
            return status;
        }
    }
    *out = stream->ptr + stream->consumedoffset;
    stream->consumedoffset +=  contentLength;
    return RECV_SUCCESS;
}

enum http_stream_status receiveData(HTTPStream *stream)
{

    int nWritten = 0;
    while ((nWritten = (int)recv(stream->connfd,
                                 stream->ptr + stream->writeoffset,
                                 stream->capacity - stream->writeoffset,
                                 MSG_NOSIGNAL)) == -1)
    {
        if (errno != EINTR)
        {
            // some other exception aside fron intterupt from signal handling
            return ERROR_RECV;
        }
    }
    stream->writeoffset += nWritten;
    if (stream->writeoffset >= stream->capacity)
    {
        if (stream->capacity >= HTTP_STREAM_MAX_BUFFER)
        {
            // buffer is much bigger than it should realistically get
            return BUF_TOO_BIG;
        }
        // grow
        // double cap
        stream->capacity <<= 1;
        // realloc
        char *new_ptr = cc_dynamic_pool_malloc(stream->capacity, stream->pool);
        // copy exisitng data to new pointer
        int nUnconsumed = stream->writeoffset - stream->consumedoffset;
        memcpy(new_ptr + stream->consumedoffset, stream->ptr + stream->consumedoffset, nUnconsumed);
        // free old
        cc_dynamic_pool_free(stream->ptr, stream->pool);
        stream->ptr = new_ptr;
    }
    return RECV_SUCCESS;
}


int initHTTPRequest(HTTPRequest *request)
{
    request->method = NULL;
    request->uri = NULL;
    request->version = NULL;
    request->contentLength = 0;
    request->body = NULL;
    CC_HashTableConf config;
    cc_hashtable_conf_init(&config);
    config.key_length = KEY_LENGTH_VARIABLE;
    config.hash = STRING_HASH;
    config.key_compare = CC_CMP_STRING;
    enum cc_stat result = cc_hashtable_new_conf(&config, &request->headers);
    if (result != CC_OK)
        return -1;
    enum cc_stat resultParams = cc_hashtable_new_conf(&config, &request->urlParams);
    if (resultParams != CC_OK)
        return -1;

    CC_DynamicPoolConf conf;
    cc_dynamic_pool_conf_init(&conf);
    conf.exp_factor = 2;
    cc_dynamic_pool_new_conf(HTTP_REQUEST_INIT, &conf, &request->pool);
    return 0;
}

void freeHTTPRequest(HTTPRequest *request)
{
    cc_hashtable_destroy(request->headers);
    cc_hashtable_destroy(request->urlParams);
    cc_dynamic_pool_destroy(request->pool);
}

int initHTTPResponse(HTTPResponse *response)
{
    response->contentLength = 0;
    response->statusCode = 0;
    response->body = NULL;
    response->headers = NULL;
    response->request = NULL;

    CC_HashTableConf config;
    cc_hashtable_conf_init(&config);
    config.key_length = KEY_LENGTH_VARIABLE;
    config.hash = STRING_HASH;
    config.key_compare = CC_CMP_STRING;
    enum cc_stat result = cc_hashtable_new_conf(&config, &response->headers);
    if (result != CC_OK)
        return -1;
    CC_DynamicPoolConf conf;
    cc_dynamic_pool_conf_init(&conf);
    conf.exp_factor = 2;
    cc_dynamic_pool_new_conf(HTTP_REQUEST_INIT, &conf, &response->pool);

    return 0;
}

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength)
{
    response->contentLength = contentLength;

    if (response->body){
      cc_dynamic_pool_free(response->body, response->pool);
    }
    response->body =cc_dynamic_pool_malloc(response->contentLength, response->pool);
    char contentLengthS[32];
    sprintf(contentLengthS, "%d", contentLength);
    setHeader(response, CONTENT_LENGTH_HEADER_NAME, contentLengthS);
}

void freeHTTPResponse(HTTPResponse *response)
{
    cc_hashtable_destroy(response->headers);
    cc_dynamic_pool_destroy(response->pool);
}
void setRequest(HTTPResponse *resp, HTTPRequest *resq)
{
    resp->request = resq;
}

void init_code_to_phrase()
{
    cc_hashtable_new(&code_to_phrase);
    cc_hashtable_add(code_to_phrase, &HTTP_OK, HTTP_OK_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_NO_CONTENT, HTTP_NO_CONTENT_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_OK, HTTP_OK_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_NO_CONTENT, HTTP_NO_CONTENT_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_MOVED, HTTP_MOVED_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_NOT_MODIFIED, HTTP_NOT_MODIFIED_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_BAD_REQUEST, HTTP_BAD_REQUEST_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_UNAUTHORIZED, HTTP_UNAUTHORIZED_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_FORBIDDEN, HTTP_FORBIDDEN_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_NOT_FOUND, HTTP_NOT_FOUND_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_METHOD_UNSUPPORTED, HTTP_METHOD_UNSUPPORTED_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_CONTENT_LENGTH_REQUIRED, HTTP_CONTENT_LENGTH_REQUIRED_PHRASE);
    cc_hashtable_add(code_to_phrase, &HTTP_SERVER_ERROR, HTTP_SERVER_ERROR_PHRASE);
}


void allocDictToArena(CC_HashTable *dict, CC_DynamicPool *pool, char *key, char *value)
{
  char *key_ptr = cc_dynamic_pool_malloc(strlen(key) + 1, pool);
  strcpy(key_ptr, key);

  char *value_ptr = cc_dynamic_pool_malloc(strlen(value) + 1, pool);
  strcpy(key_ptr, value);

  cc_hashtable_add(dict, key_ptr, value_ptr);
}
