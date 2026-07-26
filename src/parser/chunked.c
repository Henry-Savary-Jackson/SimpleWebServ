#include "cc_hashtable.h"
#include "utils.h"
#include <assert.h>
#include <http.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
int readBodyChunked(HTTPRequest *request)
{
    char *chunk = NULL;
    int chunkSize = 0;
    GrowingBuffer bodyGrowingBuffer;
    initGrowingBuffer(&bodyGrowingBuffer, HTTP_STREAM_INIT_BUFFER);

    enum http_stream_status status_strm = readNextChunk(request->inputStream, &chunk, &chunkSize);

    while ((status_strm = readNextChunk(request->inputStream, &chunk, &chunkSize)) != TRAILER_CHUNK_REACEHED)
    {
        if (status_strm != LF_REACHED)
        {
            return -1;
        }
        appendGrowingBuffer(&bodyGrowingBuffer, chunk, chunkSize);
    }
    readTrailerSection(request->inputStream, NULL);
    //
    request->body = bodyGrowingBuffer.ptr;
    request->contentLength = bodyGrowingBuffer.size;
    // start headers for trailer params
    CC_HashTableConf htConf;
    configureHTTPDict(&htConf);

    CC_HashTable *trailerParams;
    cc_hashtable_new_conf(&htConf, &trailerParams);
    return 0;
}
enum http_stream_status readNextChunk(HTTPStream *stream, char **out, int *size)
{
    const int BASE = 16;
    char *sizeS;
    char *sizeScharter;
    int sizeSLen;
    enum http_stream_status stat = readLine(stream, &sizeSLen, &sizeS);
    if (stat != LF_REACHED)
    {
        // error
        return stat;
    }
    *size = (int)strtol(sizeS, &sizeScharter, BASE);
    if (*size <= 0)
    {
        return TRAILER_CHUNK_REACEHED;
    }
    stat = consumeBody(stream, out, *size + HTTP_LINE_END_TOK_SIZE);
    if (stat != RECV_SUCCESS)
    {
        return stat;
    }
    return RECV_SUCCESS;
}
enum http_stream_status readTrailerSection(HTTPStream *stream, CC_HashTable *trailerParams)
{

    // Do nothing for now
    enum http_stream_status status;
    int lineLength;
    char *lineBuf;
    while ((status = readLine(stream, &lineLength, &lineBuf) != EMPTY_LINE))
    {
        if (status != LF_REACHED)
        {
            return status;
            // uh oh
        }
        // convert header into trailer params
    }
    return status;
}

/** send body chunks
 */
int sendChunk(int connfd, char *chunk, int size)

{
    const int maxSizeHex = 16;
    char outBuffer[size + maxSizeHex + (2 * HTTP_LINE_END_TOK_SIZE) + 1];
    snprintf(outBuffer, maxSizeHex, "%x%s", size, HTTP_LINE_END_TOK);

    int offset = (int)strlen(outBuffer);

    // add chunk data to buffer
    memcpy(outBuffer+offset, chunk, size);
    offset += size;

    // add final end CRLF
    memcpy(outBuffer+offset, HTTP_LINE_END_TOK, HTTP_LINE_END_TOK_SIZE);
    offset+= HTTP_LINE_END_TOK_SIZE;

    int result = sendDataTCP(connfd, outBuffer, offset);
    if (result < 0)
    {
        return result;
    }
    return 0;
}
int sendFinalChunk(int connfd)
{
    return sendChunk(connfd, "\0", 0);
}
int sendTrailerChunk(int connfd, CC_HashTable *trailerParams)
{
    return 0;
}
int sendBodyChunks(int connfd, HTTPResponse *response, int maxChunkSize)
{
    int currentIndex = 0;
    int contentLength = response->contentLength;
    while (currentIndex < contentLength)
    {

        int nLeft = contentLength - currentIndex;
        int size = (nLeft >= maxChunkSize) ? maxChunkSize : nLeft;
        int result = sendChunk(connfd, response->body + currentIndex, maxChunkSize);
        if (result)
        {
            return result;
        }
        currentIndex += maxChunkSize;
    }
    int result = sendFinalChunk(connfd);
    return result;
}
