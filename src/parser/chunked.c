#include "cc_hashtable.h"
#include "utils.h"
#include <assert.h>
#include <http.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
int readBodyChunked(HTTPStream *stream, HTTPRequest *request)
{

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
    readLine(stream, size, out);
    if (stat != LF_REACHED)
    {
        return stat;
    }
    return 0;
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
    char outBuffer[size + 16 + (2 * HTTP_LINE_END_TOK_SIZE) + 1];
    snprintf(outBuffer, sizeof(outBuffer), "%x\r\n%s\r\n", size, chunk);

    int result = (int)send(connfd, outBuffer, strlen(outBuffer), MSG_NOSIGNAL);
    if (result < 0)
    {
        return -1;
    }
    return 0;
}
int sendFinalChunk(int connfd){
    return sendChunk(connfd, "\0", 0);
}
int sendTrailerChunk(int connfd, CC_HashTable *trailerParams){
    return 0;
}
int sendBodyChunks(int connfd, HTTPResponse *response, int maxChunkSize){
    int currentIndex =0;
    int contentLength = response->contentLength;
    while ( currentIndex < contentLength){

        int nLeft = contentLength-currentIndex;
        int size = (nLeft >= maxChunkSize) ? maxChunkSize :nLeft;
        int result = sendChunk(connfd, response->body+ currentIndex, maxChunkSize);
        if (result){
            return result;
        }
        currentIndex += maxChunkSize;
    }
    int result = sendFinalChunk(connfd);
    return result;
}
