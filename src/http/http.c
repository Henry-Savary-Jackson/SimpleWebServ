#include "arena.h"
#include "cc_deque.h"
#include "cc_hashtable.h"
#include "memory/cc_dynamic_pool.h"
#include "server.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <errno.h>
#include <http.h>
#include <stdio.h>
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
const int HTTP_NOT_ACCEPTED = 406;
const int HTTP_CONTENT_LENGTH_REQUIRED = 411;
const int HTTP_UNSUPPORTED_MEDIA_TYPE = 415;
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
const char *HTTP_NOT_ACCEPTED_PHRASE = "Not Acceptable";
const char *HTTP_CONTENT_LENGTH_REQUIRED_PHRASE = "Length Required";
const char *HTTP_UNSUPPORTED_MEDIA_TYPE_PHRASE = "Unsupported Media Type";
const char *HTTP_SERVER_ERROR_PHRASE = "Internal Server Error";

CC_HashTable *code_to_phrase;
const char *HTTP_LINE_END_TOK = "\r\n";
const int HTTP_LINE_END_TOK_SIZE = strlen("\r\n");

const char *http_method_arr[HTTP_NUM_SUPPORTED_METHODS + 1] = {HTTP_METHOD_GET,
                                                               HTTP_METHOD_POST,
                                                               HTTP_METHOD_PUT,
                                                               HTTP_METHOD_DELETE,
                                                               HTTP_METHOD_HEAD,
                                                               HTTP_METHOD_OPTIONS,
                                                               METHOD_UNKNOWN_STR};
const char *http_encoding_arr[HTTP_NUM_SUPPORTED_ENCODINGS + 2] = {
    GZIP_HEADER_VALUE,
    ZLIB_HEADER_VALUE,
    IDENTITY_HEADER_VALUE,
    CHUNKED_HEADER_VALUE,
    WILDCARD_HEADER_VALUE,
    ENCODING_UNKNOWN_STR,
};


const char *http_content_type_arr[HTTP_NUM_SUPPORTED_CONTENT_TYPE + 1] = {
    MULTIPART_FORMDATA_VALUE,
    TEXT_HTML_VALUE,
    TEXT_PLAIN_VALUE,
};

#define HTTP_REQUEST_INIT 4096

void initHTTPStream(HTTPStream *stream, int connfd)
{
    stream->capacity = HTTP_STREAM_INIT_BUFFER;
    stream->connfd = connfd;
    stream->tokenIndex = 0;
    stream->ptr = custom_alloc(stream->capacity);
    stream->writeoffset = 0;
    stream->consumedoffset = 0;
    stream->head = 0;
}
enum http_stream_status consumeUntilLineFeed(HTTPStream *stream)
{

    bool carriageReturn = false;
    while (stream->head < stream->writeoffset)
    {
        char currentChar = stream->ptr[stream->head];
        stream->head++;

        stream->tokenIndex = (currentChar == HTTP_LINE_END_TOK[stream->tokenIndex]) ? stream->tokenIndex + 1 : 0;
        if (stream->tokenIndex >= HTTP_LINE_END_TOK_SIZE)
        {
            // set to be null terminated, so that sscanf and strlen works
            stream->ptr[stream->head - HTTP_LINE_END_TOK_SIZE] = 0;
            stream->tokenIndex = 0;
            return LF_REACHED;
        }

        if (carriageReturn)
            stream->ptr[stream->head - HTTP_LINE_END_TOK_SIZE] = ' ';
        // replace lone CR with space according to RFC 9112

        carriageReturn = !carriageReturn && currentChar == '\r'; // detect lone carriage return
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
    *lineLength = stream->head - stream->consumedoffset - HTTP_LINE_END_TOK_SIZE;
    *output = stream->ptr + stream->consumedoffset;
    stream->consumedoffset = stream->head;
    return *lineLength > 0 ? LF_REACHED : EMPTY_LINE;
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
        if (status == RECV_ERROR)
        {
            return status;
        }
    }
    *out = stream->ptr + stream->consumedoffset;
    stream->consumedoffset += contentLength;
    stream->head = stream->consumedoffset;
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
            return RECV_ERROR;
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
        char *new_ptr = custom_alloc(stream->capacity);
        // copy exisitng data to new pointer
        int nUnconsumed = stream->writeoffset - stream->consumedoffset;
        memcpy(new_ptr + stream->consumedoffset, stream->ptr + stream->consumedoffset, nUnconsumed);
        // free old
        custom_free(stream->ptr);
        stream->ptr = new_ptr;
    }
    return RECV_SUCCESS;
}

void configureHTTPDict(CC_HashTableConf *conf)
{
    cc_hashtable_conf_init(conf);
    conf->key_length = KEY_LENGTH_VARIABLE;
    conf->hash = STRING_HASH;
    conf->key_compare = CC_CMP_STRING;
    conf->mem_alloc = custom_alloc;
    conf->mem_calloc = custom_calloc;
    conf->mem_free = custom_free;
}


int initHTTPRequest(HTTPRequest *request)
{
    request->method = GET;
    request->uri = NULL;
    request->version = NULL;
    request->contentLength = 0;
    request->body = NULL;
    request->transferEncoding = IDENTITY_ENCODING;
    request->contentEncoding = IDENTITY_ENCODING;
    initPath(&request->uriPath);

    CC_HashTableConf htConf;
    configureHTTPDict(&htConf);

    enum cc_stat result = cc_hashtable_new_conf(&htConf, &request->headers);
    if (result != CC_OK)
    {
        return -1;
    }
    enum cc_stat resultParams = cc_hashtable_new_conf(&htConf, &request->urlParams);
    if (resultParams != CC_OK)
    {
        return -1;
    }
    enum cc_stat resultCookies = cc_hashtable_new_conf(&htConf, &request->cookies);
    if (resultParams != CC_OK)
    {
        return -1;
    }

    return 0;
}

void freeHTTPRequest(HTTPRequest *request)
{
    // cc_dynamic_pool_destroy(request->pool);
}

int initHTTPResponse(HTTPResponse *response)
{
    response->contentLength = 0;
    response->statusCode = HTTP_OK;
    response->body = NULL;
    response->headers = NULL;
    response->request = NULL;
    response->contentEncoding = IDENTITY_ENCODING;
    response->transferEncoding = IDENTITY_ENCODING;
    response->contentType = NULL;

    CC_HashTableConf htConf;
    configureHTTPDict(&htConf);

    enum cc_stat result = cc_hashtable_new_conf(&htConf, &response->headers);
    if (result != CC_OK)
    {
        return -1;
    }
    enum cc_stat resultCookies = cc_hashtable_new_conf(&htConf, &response->cookies);
    if (resultCookies != CC_OK)
    {
        return -1;
    }

    return 0;
}

void setResponseBody(HTTPResponse *response, char *buffer, int contentLength)
{
    response->contentLength = contentLength;


    response->body = custom_alloc(response->contentLength);
    memcpy(response->body, buffer, contentLength);

    // char contentLengthS[32];
    // sprintf(contentLengthS, "%d", contentLength);
    // setHeader(response, CONTENT_LENGTH_HEADER_NAME, contentLengthS);
}


void freeHTTPResponse(HTTPResponse *response)
{
    // cc_dynamic_pool_destroy(response->pool);
}
void setRequest(HTTPResponse *resp, HTTPRequest *resq)
{
    resp->request = resq;
}

void init_code_to_phrase()
{
    cc_hashtable_new(&code_to_phrase);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_OK, (char *)HTTP_OK_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_NO_CONTENT, (char *)HTTP_NO_CONTENT_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_OK, (char *)HTTP_OK_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_NO_CONTENT, (char *)HTTP_NO_CONTENT_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_MOVED, (char *)HTTP_MOVED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_NOT_MODIFIED, (char *)HTTP_NOT_MODIFIED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_BAD_REQUEST, (char *)HTTP_BAD_REQUEST_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_UNAUTHORIZED, (char *)HTTP_UNAUTHORIZED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_FORBIDDEN, (char *)HTTP_FORBIDDEN_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_NOT_FOUND, (char *)HTTP_NOT_FOUND_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_METHOD_UNSUPPORTED, (char *)HTTP_METHOD_UNSUPPORTED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_NOT_ACCEPTED, (char *)HTTP_NOT_ACCEPTED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_CONTENT_LENGTH_REQUIRED, (char *)HTTP_CONTENT_LENGTH_REQUIRED_PHRASE);
    cc_hashtable_add(code_to_phrase, (int *)&HTTP_SERVER_ERROR, (char *)HTTP_SERVER_ERROR_PHRASE);
}

char *getValueDict(CC_HashTable *dict, char *key)
{
    //
    char *value;
    CC_Deque *list;
    enum cc_stat stat = cc_hashtable_get(dict, key, (void **)&list);
    if (stat == CC_ERR_KEY_NOT_FOUND)
    {
        return NULL;
    }
    char *out;
    cc_deque_get_first(list, (void **)&out);
    return out;
}

CC_Deque *getValuesDict(CC_HashTable *dict, char *key)
{
    CC_Deque *deque = NULL;
    enum cc_stat stat = cc_hashtable_get(dict, key, (void **)&deque);
    if (stat == CC_ERR_KEY_NOT_FOUND)
    {
        return NULL;
    }
    return deque;
}

void putKVDict(CC_HashTable *dict, char *key, char *value)
{
    CC_Deque *valueQueue = getValuesDict(dict, key);
    if (valueQueue == NULL)
    {
        CC_DequeConf conf;
        cc_deque_conf_init(&conf);
        conf.mem_alloc = custom_alloc;
        conf.mem_calloc = custom_calloc;
        conf.mem_free = custom_free;
        cc_deque_new_conf(&conf, &valueQueue);
        key = custom_strdup(key);
        cc_hashtable_add(dict, key, valueQueue);
    }
    cc_deque_add_last(valueQueue, custom_strdup(value));
}
