#include "cc_array.h"
#include "cc_common.h"
#include "cc_deque.h"
#include "cc_hashtable.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "server.h"
#include "utils.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <complex.h>
#include <errno.h>
#include <linux/limits.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 4096
#define BUFFER_INCREMENTS 4096
#define INIT_BUFFER_SIZE 16
#define METHOD_MAX_SIZE 16

void handleQueryParam(char *inStr, void *args)
{
    int lineLength = (int)strlen(inStr);
    HTTPRequest *request = args;
    char key[lineLength];
    char value[lineLength];
    int count = sscanf(inStr, "%[^=]=%s", key, value);
    if (count != 2)
    {
        perror("invalid query params");
        return;
    }
    // add the dictionary
    setQueryParam(request, key, value);
}

int parseQueryParameters(char *queryParams, HTTPRequest *request)
{
    tokenize(queryParams, '&', handleQueryParam, (void*)request) ;
    return 0;
}

int scanFirstLine(HTTPRequest *request)
{
    char *firstLine = NULL;
    int lineLength = 0;
    enum http_stream_status status = readLine(request->inputStream, &lineLength, &firstLine);
    if (status != LF_REACHED)
    {
        return -1;
    }

    assert(firstLine);

    char uri[PATH_MAX];
    char queryParams[lineLength];
    char location[lineLength];
    char method[METHOD_MAX_SIZE];
    char version[METHOD_MAX_SIZE];

    uint count = sscanf(request->inputStream->ptr, "%15s %s HTTP/%15s", method, location, version);
    if (count != 3)
    {
        return -1;
    }
    count = sscanf(location, "%[^?]?%s", uri, queryParams);
    if (count >= 2)
    {
        // query paramters detected
        int result = parseQueryParameters(queryParams, request);
        if (result)
        {
            return -1;
        }
    }

    request->method = HTTP_METHOD(method);

    request->uri = custom_strdup(uri);

    request->version = custom_strdup(version);

    stringToPath(&request->uriPath, request->uri);
    return 0;
}

int scanHeaders(HTTPRequest *request)
{
    int lineLength = 0;
    char *line = NULL;
    enum http_stream_status streamStatus;
    while ((streamStatus = readLine(request->inputStream, &lineLength, &line)) != EMPTY_LINE)
    {
        if (streamStatus != LF_REACHED)
        {
            // an error occurred
            return -1;
        }
        assert(lineLength > 0);
        // parse current line to a header key value pair
        // allocate a new key val pair
        char key[lineLength];
        char value[lineLength];

        sscanf(line, "%[^:]: ", key);
        strcpy(value, line + strlen(key) + strlen(": "));

        setHeader(request, key, value);
    }
    return 0;
}

int scanBody(HTTPRequest *request)
{
    // read body of message using content length
    enum http_stream_status strStatus = consumeBody(request->inputStream, &request->body, request->contentLength);
    if (strStatus != RECV_SUCCESS)
    {
        return -1;
    }
    return 0;
}

int decompressBody(HTTPRequest *request, enum http_encoding encoding)
{
    if (!request->body || request->contentLength <= 0)
    {
        return 0;
    }
    int ret = 0;
    char *newPtr = NULL;
    switch (encoding)
    {
    case GZIP:
        ret = decode_gzip(request->body, request->contentLength, &newPtr, &request->contentLength);
        if (ret)
        {
            return ret;
        }
        request->body = newPtr;
        break;
    case DEFLATE:
        ret = decode_zlib(request->body, request->contentLength, &newPtr, &request->contentLength);
        if (ret)
        {
            return ret;
        }
        request->body = newPtr;
        break;
    default:
        break;
    }
    return ret;
}


int prepareHTTPRequestMetadata(HTTPRequest *request, int *status, bool* keepAlive)
{
    char *contentLengthS = getHeader(request->headers, CONTENT_LENGTH_HEADER_NAME);
    char *contentEncodingS = getHeader(request->headers, CONTENT_ENCODING_HEADER_NAME);

    // CC_Deque *transferCodingValues = getHeaderValues(request->headers, TRANSFER_CODING_HEADER_NAME);
    if (!contentEncodingS)
    {
        request->contentEncoding = IDENTITY_ENCODING;
    }
    else
    {
        request->contentEncoding = HTTP_ENCODING(contentEncodingS);
        if (request->contentEncoding == UNKNOWN_ENCODING)
        {
            *status = HTTP_UNSUPPORTED_MEDIA_TYPE;
            return -1;
        }
    }

    if (contentLengthS)
    {
        const int base = 10;
        request->contentLength = (int)strtol(contentLengthS, NULL, base);
    }
    return 0;
}

int scanRequest(HTTPRequest *request, bool *keepAlive, int *status)
{

    // parse the first line
    int result = scanFirstLine(request);
    if (result)
    {
        *status = HTTP_BAD_REQUEST;
        printf("Error scanning first line!");
        goto error;
    }

    stringToPath(&request->uriPath, request->uri);

    result = scanHeaders(request);
    if (result)
    {
        *status = HTTP_BAD_REQUEST;
        goto error;
    }

    // the Host header is required

success:
    char *connect = getHeader(request->headers, CONNECTION_HEADER_NAME);
    *keepAlive = (connect && !strcmp(connect, "keep-alive"));
    *status = HTTP_OK;
    return 0;
error:
    return -1;
}

int prepareHTTPResponseStatusLine(HTTPResponse *response, GrowingBuffer *buffer)
{
    char firstline[4096];
    int totalSize = 0;

    char *phrase;
    enum cc_stat stat_phrase = cc_hashtable_get(code_to_phrase, &response->statusCode, (void **)&phrase);
    assert(stat_phrase == CC_OK);

    sprintf(firstline, "HTTP/%s %d %s\r\n", response->request->version, response->statusCode, phrase);

    int firstLineLength = (int)strlen(firstline);
    appendGrowingBuffer(buffer, firstline, firstLineLength);
    return 0;
}

int prepareHTTPResponseMetadata(HTTPResponse *response)
{
    char contentLengthS[METHOD_MAX_SIZE];
    sprintf(contentLengthS, "%d", response->contentLength);
    if (response->transferEncoding != CHUNKED){
        setHeader(response, CONTENT_LENGTH_HEADER_NAME, contentLengthS);
    }
    if (response->contentLength > 0 && response->contentEncoding != IDENTITY_ENCODING)
    {
        setHeader(response, CONTENT_TYPE_HEADER_NAME, response->contentType);
        setHeader(response, CONTENT_ENCODING_HEADER_NAME, HTTP_ENCODING_STRING(response->contentEncoding));
    }
    if (response->transferEncoding != IDENTITY_ENCODING)
    {
        setHeader(response, TRANSFER_CODING_HEADER_NAME, HTTP_ENCODING_STRING(response->transferEncoding));
    }
    response->version = "1.1";

    return 0;
}

int compressReponseBody(HTTPResponse *response, enum http_encoding encoding)
{
    if (!response->body || response->contentLength <= 0)
    {
        return 0;
    }
    char *newBodyPtr = NULL;
    int ret = 0;
    int newSize = 0;
    switch (encoding)
    {
    case GZIP:
        ret = encode_gzip(response->body, response->contentLength, &newBodyPtr, &newSize);
        break;
    case DEFLATE:
        ret = encode_zlib(response->body, response->contentLength, &newBodyPtr, &newSize);
        break;
    default:
        return 0;
    }
    if (!ret)
    {
        response->body = newBodyPtr;
        response->contentLength = newSize;
    }
    return ret;
}

int prepareResponseBody(HTTPResponse *response)
{
    int ret = compressReponseBody(response, response->contentEncoding);
    if (ret)
    {
        return ret;
    }
    return 0;
}

int sendResponse(HTTPResponse *response, int connfd)
{
    GrowingBuffer outBuffer;

    initGrowingBuffer(&outBuffer, INIT_BUFFER_SIZE);

    prepareHTTPResponseStatusLine(response, &outBuffer);
    prepareResponseBody(response);
    prepareHTTPResponseMetadata(response);

    encodeHeaders(response, &outBuffer);
    encodeResponseBody(response, &outBuffer);

    return sendDataTCP(connfd, outBuffer.ptr, outBuffer.size);
}

int encodeResponseBody(HTTPResponse *response, GrowingBuffer *buffer)
{
    if (response->body && response->contentLength > 0)
    {
        appendGrowingBuffer(buffer, response->body, response->contentLength);
    }
    return 0;
}

int encodeHeaders(HTTPResponse *response, GrowingBuffer *buffer)
{
    CC_HashTable *headers = response->headers;
    int numHeaders = (int)cc_hashtable_size(headers);
    struct cc_hashtable_iter iter;
    cc_hashtable_iter_init(&iter, headers);

    enum cc_stat stat;
    TableEntry *currentEntry;
    while ((stat = cc_hashtable_iter_next(&iter, &currentEntry)) != CC_ITER_END)
    {

        CC_Deque *list = currentEntry->value;
        char *value;
        cc_deque_get_first(list, (void **)&value);
        int maxLineLength = (int)(strlen(": \r\n") + strlen(currentEntry->key) + strlen(value)) + 1;
        char currentLine[maxLineLength]; //
        snprintf(currentLine, maxLineLength, "%s: %s\r\n", (char *)currentEntry->key, value);
        int lineLength = (int)strlen(currentLine);
        appendGrowingBuffer(buffer, currentLine, lineLength);
    }
    appendGrowingBuffer(buffer, (char *)HTTP_LINE_END_TOK, HTTP_LINE_END_TOK_SIZE);

    return 0;
}
