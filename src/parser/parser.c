#include "cc_array.h"
#include "cc_common.h"
#include "cc_hashtable.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
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
#define INIT_BUFFER_SIZE 2048
#define METHOD_MAX_SIZE 16

int parseQueryParameters(char *queryParams, HTTPRequest *request)
{
    int consumedOffset = 0;
    int length = (int)strlen(queryParams);
    while (consumedOffset < length)
    {
        int readHead = consumedOffset;
        while (queryParams[readHead] != '&' && length > readHead)
        {
            readHead++;
        }
        //
        // null terminate
        int lineLength = readHead - consumedOffset;
        if (lineLength <= 0)
        {
            return -1;
        }

        queryParams[readHead] = 0;
        char key[lineLength];
        char value[lineLength];
        int count = sscanf(queryParams + consumedOffset, "%[^=]=%s", key, value);
        if (count != 2)
        {
            return -1;
        }
        // add the dictionary
        setQueryParam(request, key, value);

        consumedOffset = readHead + 1;
    }
    return 0;
}

int scanFirstLine(HTTPStream *stream, HTTPRequest *request)
{

    char *firstLine = NULL;
    int lineLength = 0;
    enum http_stream_status status = readLine(stream, &lineLength, &firstLine);
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

    uint count = sscanf(stream->ptr, "%15s %s HTTP/%s", method, location, version);
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

    int methodLen = strlen(method);
    int uriLen = strlen(uri);
    int versionlen = strlen(version);

    request->method = cc_dynamic_pool_malloc(methodLen, request->pool);
    memcpy(request->method, method, methodLen + 1);

    request->uri = cc_dynamic_pool_malloc(strlen(uri), request->pool);
    memcpy(request->uri, method, uriLen + 1);

    request->version = cc_dynamic_pool_malloc(strlen(version), request->pool);
    memcpy(request->version, version, versionlen + 1);


    return 0;
}

int scanHeaders(HTTPStream *stream, HTTPRequest *request)
{
    int lineLength = 0;
    char *line = NULL;
    enum http_stream_status streamStatus;
    while ((streamStatus = readLine(stream, &lineLength, &line)) != EMPTY_LINE)
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

        sscanf(line, "%[^:]: %s", key, value);

        setHeader(request, key, value);
    }
    return 0;
}

int scanBody(HTTPStream *stream, HTTPRequest *request, int *status)
{
    char *contentLengthS;
    enum cc_stat stat = cc_hashtable_get(request->headers, CONTENT_LENGTH_HEADER_NAME, (void **)&contentLengthS);
    bool hasContLength = stat == CC_OK;

    char *transferCodingS;
    stat = cc_hashtable_get(request->headers, TRANSFER_CODING_HEADER_NAME, (void **)&transferCodingS);
    bool hasTransferCoding = stat == CC_OK;

    if (!strcmp(HTTP_METHOD_GET, request->method) && !hasContLength)
    {
        // ignore message body if a get request
        return 0;
    }
    if (!hasContLength)
    {
        *status = HTTP_CONTENT_LENGTH_REQUIRED;
        return -1;
        // read message body into request buffer
    }
    request->contentLength = atoi(contentLengthS);
    // read body of message using content length

    enum http_stream_status strStatus = consumeBody(stream, &request->body, request->contentLength);

    if (strStatus != RECV_SUCCESS)
    {
        return -1;
    }
    return 0;
}

int scanRequest(int connfd, HTTPRequest *request, bool *keepAlive, int *status)
{
    HTTPStream stream;
    initHTTPStream(&stream, connfd, request->pool);

    // parse the first line
    int result = scanFirstLine(&stream, request);
    if (result)
    {
        *status = HTTP_BAD_REQUEST;
        printf("Error scanning first line!");
        goto error;
    }

    result = scanHeaders(&stream, request);
    if (result)
    {
        *status = HTTP_BAD_REQUEST;
        goto error;
    }
    // the Host header is required
    bool hasHost = cc_hashtable_contains_key(request->headers, HOST_HEADER_NAME);
    if (!hasHost)
    {
        *status = HTTP_BAD_REQUEST;
        printf("Host header missing!");
        goto error;
    }
    scanBody(&stream, request, status);
    goto success;
success:
    char *connect;
    enum cc_stat stat_conn = cc_hashtable_get(request->headers, CONNECTION_HEADER_NAME, (void **)&connect);
    *keepAlive = ((stat_conn == CC_OK && !strcmp(connect, "keep-alive")) != 0);
    *status = HTTP_OK;
    return 0;
error:
    return -1;
}

int sendResponse(HTTPResponse *response, int connfd)
{
    GrowingBuffer outBuffer;
    initGrowingBuffer(&outBuffer, response->pool, INIT_BUFFER_SIZE);

    char firstline[MAX_BUFFER_SIZE];
    int totalSize = 0;

    char *phrase;
    enum cc_stat stat_phrase = cc_hashtable_get(code_to_phrase, &response->statusCode, (void **)&phrase);
    assert(stat_phrase == CC_OK);

    sprintf(firstline, "HTTP/%s %d %s\r\n", response->request->version, response->statusCode, phrase);

    int firstLineLength = (int)strlen(firstline);
    appendGrowingBuffer(&outBuffer, firstline, firstLineLength);

    encodeHeaders(response->headers, &outBuffer);

    if (response->body && response->contentLength > 0)
    {
      appendGrowingBuffer(&outBuffer, response->body, response->contentLength);
    }
    send(connfd, outBuffer.ptr, outBuffer.size, MSG_NOSIGNAL);
    return 0;
}

int encodeHeaders(CC_HashTable *headers, GrowingBuffer *buffer)
{
    int numHeaders = (int)cc_hashtable_size(headers);
    struct cc_hashtable_iter iter;
    cc_hashtable_iter_init(&iter, headers);

    enum cc_stat stat;
    TableEntry *currentEntry;
    while ((stat = cc_hashtable_iter_next(&iter, &currentEntry)) != CC_ITER_END)
    {
        int maxLineLength = (int)(strlen(": \r\n") + strlen(currentEntry->key) + strlen(currentEntry->value));
        char currentLine[maxLineLength]; //
        sprintf(currentLine, "%s: %s\r\n", (char *)currentEntry->key, (char *)currentEntry->value);
        int lineLength = (int)strlen(currentLine);
        appendGrowingBuffer(buffer, currentLine, lineLength);
    }
    appendGrowingBuffer(buffer, (char *)HTTP_LINE_END_TOK, HTTP_LINE_END_TOK_SIZE);

    return 0;
}
