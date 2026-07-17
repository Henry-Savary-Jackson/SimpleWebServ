
#include "arena.h"
#include "http.h"
#include "stb_ds.h"
#include "utils.h"
#include <asm-generic/errno-base.h>
#include <complex.h>
#include <errno.h>
#include <linux/limits.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_BUFFER_SIZE 4096
#define METHOD_MAX_SIZE 16

bool parseLine(char *dst, int writeOffset, char *src, int *readOffset)
{
    // return whether the line has terminated
    const char *token = "\r\n";
    int tokenSize = strlen(token);
    int tokenIndex = 0;
    // trying to follow RFC9112

    bool cr = false;
    int i = 0;
    while (*readOffset + i < writeOffset)
    {
        char c = src[*readOffset + i];
        dst[i] = c;
        i++;

        tokenIndex = (c == token[tokenIndex]) ? tokenIndex + 1 : 0;
        if (tokenIndex >= tokenSize)
        {
            // set to be null terminated, so that sscanf and strlen works
            dst[i - 2] = 0;
            // return with true to indicate that line has been parsed
            *readOffset += i;
            return true;
        }

        if (cr)
            dst[i - 2] = ' ';
        // replace lone CR with space according to RFC

        cr = !cr && c == '\r'; // detect lone carriage return
    }
    *readOffset += i;
    return false;
}


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

int scanFirstLine(int connfd, HTTPRequest *request, char *buffer, int *readOffset, int *writeOffset)
{
    do
    {
        *writeOffset += (int)recv(connfd, buffer, MAX_BUFFER_SIZE - *writeOffset, 0);
    } while (!parseLine(buffer, *writeOffset, buffer, readOffset));

    char uri[PATH_MAX];
    char *queryParams = NULL;
    char *location = NULL;
    char method[METHOD_MAX_SIZE];
    char version[METHOD_MAX_SIZE];

    uint count = sscanf(buffer, "%15s %ms HTTP/%s", method, &location, version);
    if (count != 3)
    {
        return -1;
    }
    count = sscanf(location, "%[^?]?%ms", uri, &queryParams);
    if (count >= 2)
    {
        // query paramters detected
        int result = parseQueryParameters(queryParams, request);
        if (result)
        {
            return -1;
        }
    }

    request->method = arena_strdup(&request->arena, method);
    request->uri = arena_strdup(&request->arena, uri);
    request->version = arena_strdup(&request->arena, version);

    return 0;
}

int scanHeaders(int connfd, HTTPRequest *request, char *buffer, int *readoffset, int *writeoffset, int *status)
{
    while (true)
    {
        // keep recv until line is constructed
        int tempReadOffset = *readoffset;
        while (tempReadOffset < *writeoffset && !parseLine(buffer + *readoffset, *writeoffset, buffer, &tempReadOffset))
        {
            // if the write offset into the buffer is at the maximum, move all unread
            // data to the start of the buffer;
            if (*writeoffset >= MAX_BUFFER_SIZE)
            {
                int n_chars_left = *writeoffset - *readoffset;
                memmove(buffer, buffer + *readoffset, n_chars_left);
                // zero the right
                memset(buffer + n_chars_left, 0, MAX_BUFFER_SIZE - n_chars_left);
                *writeoffset = n_chars_left;
                tempReadOffset = 0;
                *readoffset = 0;
            }
            int nWritten = 0;
            while ((nWritten = (int)recv(connfd, buffer, MAX_BUFFER_SIZE - *writeoffset, 0)) == -1)
            {
                if (errno != EINTR)
                {
                    // some other exception;
                    *status = HTTP_SERVER_ERROR;
                    return -1;
                }
            }
            writeoffset += nWritten;
        }
        int nBytesLine = tempReadOffset - 2 - *readoffset; // minus length of CRLF
        if (nBytesLine <= 0)
        {
            // if the line has 0 bytes we have finished reading the header section
            *readoffset = tempReadOffset;
            break;
        }

        // parse current line to a header key value pair
        // allocate a new key val pair
        char key[nBytesLine];
        char value[nBytesLine];

        sscanf(buffer + *readoffset, "%[^:]: %s", key, value);

        setHeader(request, key, value);

        // update the readoffset to end of current line
        *readoffset = tempReadOffset;
    }
    return 0;
}

int scanBody(int connfd, HTTPRequest *request, int *status, char *buffer, int readoffset, int writeoffset)
{
    char *hasContLength = shget(request->headers, CONTENT_LENGTH_HEADER_NAME);
    char *hasTransferCoding = shget(request->headers, TRANSFER_CODING_HEADER_NAME);
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
    assert(hasContLength);
    // convert content length str to int;
    request->contentLength = atoi(hasContLength);
    // read body of message using content length
    request->body = arena_alloc(&request->arena, request->contentLength);

    if (writeoffset > readoffset)
    {
    // read the rest of the into readoffset
        memcpy(request->body, buffer + readoffset, writeoffset - readoffset);
    }
    int nrecv = 0;

    int nTotalWritten = writeoffset - readoffset;
    nTotalWritten = nTotalWritten > 0 ? nTotalWritten : 0;

    int contentLength = request->contentLength;

    while (nTotalWritten < contentLength &&
           ((nrecv = (int)recv(connfd, request->body + nTotalWritten, contentLength - nTotalWritten, MSG_NOSIGNAL)) == -1))
    {
        // if the error code is simply the handling of a signal, then just
        // continue
        bool isEint = errno == EINTR;
        if (nrecv == -1 && !isEint)
        {
            *status = HTTP_SERVER_ERROR;
            return -1;
        }
        if (!isEint)
        {
            // if no error code is present, continue receiving data
            nTotalWritten += nrecv;
        }
    }

    return 0;
}

int scanRequest(int connfd, HTTPRequest *request, bool *keepAlive, int *status)
{
    char buffer[MAX_BUFFER_SIZE];
    int readoffset = 0;
    int writeoffset = 0;
    // parse the first line
    int result = scanFirstLine(connfd, request, buffer, &readoffset, &writeoffset);
    if (result)
    {
        printf("Error scanning first line!");
        goto error;
    }

    result = scanHeaders(connfd, request, buffer, &readoffset, &writeoffset, status);
    if (result)
    {
        goto error;
    }
    // the Host header is required
    char *hasHost = shget(request->headers, HOST_HEADER_NAME);
    if (!hasHost)
    {
        *status = HTTP_BAD_REQUEST;
        printf("Host header missing!");
        goto error;
    }
    scanBody(connfd, request, status, buffer, readoffset, writeoffset);
success:
    char *connect = shget(request->headers, CONNECTION_HEADER_NAME);
    *keepAlive = ((connect && !strcmp(connect, "keep-alive")) != 0);
    *status = HTTP_OK;
    return 0;
error:
    return -1;
}

int sendResponse(HTTPResponse *response, int connfd)
{
    char *outBuffer = NULL;

    char firstline[MAX_BUFFER_SIZE];


    sprintf(firstline,
            "HTTP/%s %d %s\r\n",
            response->request->version,
            response->statusCode,
            hmget(code_to_phrase, response->statusCode));

    int firstLineLength = strlen(firstline);
    char *ptrHeaders = arraddnptr(outBuffer, firstLineLength);
    memcpy(outBuffer, firstline, firstLineLength);
    // allocate buffer for content lenght
    // make sure content-length is present

    encodeHeaders(response->headers, &outBuffer);

    if (response->body && response->contentLength > 0)
    {
        char *bodyPtr = arraddnptr(outBuffer, response->contentLength);
        memcpy(bodyPtr, response->body, response->contentLength);
    }
    int totalLen = arrlen(outBuffer);
    send(connfd, outBuffer, totalLen, MSG_NOSIGNAL);

    arrfree(outBuffer);
    return 0;
}

int encodeHeaders(Header *headers, char **buffer)
{
    char currentLine[(MAX_BUFFER_SIZE * 2) + 20]; //

    for (int i = 0; i < shlen(headers); i++)
    {
        sprintf(currentLine, "%s: %s\r\n", headers[i].key, headers[i].value);
        int lineLength = strlen(currentLine);
        char *dst = arraddnptr(*buffer, lineLength);
        memcpy(dst, currentLine, lineLength);
    }
    char *dst = arraddnptr(*buffer, 2);
    memcpy(dst, "\r\n", 2); // 3 to incldue \0 at the end

    return 0;
}
