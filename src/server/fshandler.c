#include "cc_array.h"
#include "cc_common.h"
#include "cc_deque.h"
#include "cc_pqueue.h"
#include "cc_queue.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "parser.h"
#include "utils.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <auth.h>
#include <errno.h>
#include <linux/limits.h>
#include <server.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zconf.h>
#include <zlib.h>


#define FS_CHUNK_SIZE 2 << 12 // 4 kb

void sanitizeURI(char *uri, char *output)
{
    int bufLen = strlen(uri);
    int headOut = 0;
    int headIn = 0;
    bool dotFound = false;
    while (headIn < bufLen)
    {
        char c = uri[headIn];
        headIn++;
        bool isDot = c == '.';
        if (dotFound && isDot)
        {
            dotFound = false;
            headOut--;
            continue;
        }
        dotFound = ((isDot && !dotFound) != 0);
        output[headOut] = c;
        headOut++;
    }
    output[headOut] = 0; // nul terminate
}

int handleDirectory(Path *fullPath, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{
    addToPath(fullPath, "index.html");
    return 0;
}

int handleNotFound(Path *fullPath, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{
    response->statusCode = HTTP_NOT_FOUND;
    const char *respStr = "Not Found!";
    response->body = custom_strdup((char *)respStr);
    response->contentLength = (int)strlen(respStr);
    return -1;
}

int chooseContentType(char *path, HTTPRequest *request, HTTPResponse *response, int connfd)
{
    // check if mimetype desire matches
    // check if
    CC_Deque *acceptMimetypes = getHeaderValues(request, ACCEPT_MIMETYPE_HEADER_NAME);
    if (acceptMimetypes != NULL)
    {
        // there are specified content headers

        char *mimetype = getMimeTypeForFile(path);
        CC_PQueue *accept_mimetypes = decodeAcceptTypes(acceptMimetypes);
        assert(accept_mimetypes);
        char *chosenMimetype = NULL;

        int result = decideContentType(accept_mimetypes, mimetype, &chosenMimetype);
        if (result != 0)
        {
            // error
            response->statusCode = HTTP_NOT_ACCEPTED;
            response->contentType = "text/html";
            response->contentEncoding = IDENTITY_ENCODING;
            response->transferEncoding = IDENTITY_ENCODING;
            sendResponse(response, connfd);
            return -1;
        }
        decodeRequestContentMimeType(chosenMimetype, &response->mediaType);
        setContentType(response, chosenMimetype);
    }
    else
    {
        response->contentType = "text/html";
    }
    return 0;
}


int readChunksIntoFileDecompress(z_streamp strm,
                                 FILE *file,
                                 HTTPRequest *request,
                                 FileSystemHandler *handler,
                                 int connfd)
{
    char *chunk = NULL;
    int chunkSize = 0;
    char outputChunk[FS_CHUNK_SIZE];
    enum http_stream_status status_strm;
    while ((status_strm = readNextChunk(request->inputStream, &chunk, &chunkSize)) != TRAILER_CHUNK_REACEHED)
    {
        if (status_strm != RECV_SUCCESS)
        {
            // error
            return -1;
        }
        strm->avail_in = chunkSize;
        strm->next_in = (Bytef *)chunk;
        while (strm->avail_in > 0)
        {
            int n_inflated = inflateChunk(strm, sizeof(outputChunk), outputChunk);
            if (n_inflated < 0)
            {
                return n_inflated;
            }
            int ret = writeToFile(file, outputChunk, n_inflated);
            if (ret)
            {
                return -1;
            }
        }
    }
    readTrailerSection(request->inputStream, NULL);
    return 1;
}


int readChunksIntoFile(FILE *file, HTTPRequest *request, FileSystemHandler *handler, int connfd)
{
    char *chunk = NULL;
    int chunkSize = 0;
    enum http_stream_status status_strm;
    while ((status_strm = readNextChunk(request->inputStream, &chunk, &chunkSize)) != TRAILER_CHUNK_REACEHED)
    {
        if (status_strm != RECV_SUCCESS)
        {
            // error/
            return -1;
        }
        int ret = writeToFile(file, chunk, chunkSize);
        if (ret)
        {
            return -1;
        }
    }
    readTrailerSection(request->inputStream, NULL);
    return 1;
}
int handleChunkedTransferCoding(FILE *file, HTTPRequest *request, FileSystemHandler *handler, int connfd)
{
    z_stream strm;
    int ret = 0;
    switch (request->contentEncoding)
    {
    case GZIP:
        ret = decode_gzip_prepare(&strm);
        if (ret != Z_OK)
        {
            return -1;
        }
        break;
    case DEFLATE:
        ret = decode_zlib_prepare(&strm);
        if (ret != Z_OK)
        {
            return -1;
        }
        break;
    default:
        return readChunksIntoFile(file, request, handler, connfd);
        // just normal
    }
    return readChunksIntoFileDecompress(&strm, file, request, handler, connfd);
}

int handleTransferCodingRequest(FILE *file,
                                HTTPRequest *request,
                                HTTPResponse *response,
                                FileSystemHandler *handler,
                                int connfd)
{
    switch (request->transferEncoding)
    {
    case CHUNKED:
        return handleChunkedTransferCoding(file, request, handler, connfd);
    default:
        return decompressBody(request, request->transferEncoding);
    }
}

int handleDELETEFile(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    int ret = 0;
    char pathStr[PATH_MAX];
    Path *path = &request->uriPath;
    pathToStr(path, pathStr);

    if (access(pathStr, F_OK))
    {
        makeNotFound(response, "File not found!");
        goto end;
    }

    struct stat stat_res;

    stat(pathStr, &stat_res);

    bool isDir = S_ISDIR(stat_res.st_mode);

    ret = isDir ? rmdir(pathStr) : unlink(pathStr);

    response->statusCode = HTTP_OK;
    response->contentEncoding = IDENTITY_ENCODING;
    response->transferEncoding = IDENTITY_ENCODING;

    sendResponse(response, connfd);
end:
    return ret;
}


int handleRequestContentEncoding(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char *newBodyPtr = NULL;
    int newSize = 0;
    int ret = 0;
    switch (request->contentEncoding)
    {
    case GZIP:
        ret = decode_gzip(request->body, request->contentLength, &newBodyPtr, &newSize);
        if (ret)
        {
            return ret;
        }
        break;
    case DEFLATE:
        ret = decode_zlib(request->body, request->contentLength, &newBodyPtr, &newSize);
        if (ret)
        {
            return ret;
        }
        break;
    default:
        return 0;
    }

    request->body = newBodyPtr;
    request->contentLength = newSize;

    return 0;
}

int handlePOSTFile(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    FILE *openedFile = NULL;
    Path *path = &request->uriPath;
    char pathStr[PATH_MAX];
    int ret = 0;
    do
    {
        pathToStr(path, pathStr);
        openedFile = fopen(pathStr, "w+");
        if (openedFile == NULL)
        {
            bool tryAgain = false;

            switch (errno)
            {
            case EINTR:
                // process was just handling a signal, unlikely to happen
                tryAgain = true;
                break;
            default:
                break;
            }
        }
    } while (openedFile == NULL);

    if (!openedFile)
    {
        // still null, it failed, send response back
        makeNotFound(response, "File not found!");
        ret = -1;
        goto finish;
    }

    ret = handleTransferCodingRequest(openedFile, request, response, handler, connfd);

    if (ret > 0)
    {
        ret = 0;
        // done if you wrote to the file chunked
        goto finish;
    }

    if (ret)
    {
        makeBadRequest(response, "Failed with Transfer-encoding!");
        goto finish;
    }

    if (request->contentLength == 0)
    {
        unlink(pathStr);
        ret = mkdir(pathStr, 0777);
        goto finish;
    }

    ret = handleRequestContentEncoding(request, response, handler, connfd);

    if (ret)
    {
        makeBadRequest(response, "Failed with Content Encoding!");
        goto success;
    }
    // if you didnt using chunked, then start writing the final body
    ret = writeToFile(openedFile, request->body, request->contentLength);

    if (ret)
    {

        makeServerError(response, "Failed to write to file!");
        goto finish;
    }

success:
    sendResponse(response, connfd);
finish:
    if (openedFile)
    {
        fclose(openedFile);
    }
    return ret;
}

int sendBodyGETChunked(FILE *file, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char chunk[FS_CHUNK_SIZE];
    int n_read = 0;
    int ret = 0;

    while (!feof(file))
    {

        n_read = readFromFile(file, chunk, sizeof(chunk));
        int end = feof(file);
        if (n_read < 0)
        {
            ret = -1;
            goto end_code;
        }
        int ret = sendChunk(connfd, chunk, n_read);
        if (ret < 0)
        {
            goto end_code;
        }
    }

end_code:
    sendFinalChunk(connfd);
    return 0;
}

int sendChunkCompressed(z_stream *strm, char *chunk, int size, int connfd, bool eof)
{
    int ret = 0;
    strm->avail_in = size;
    strm->next_in = (Bytef *)chunk;
    char outChunk[compressBound(size)];
    strm->next_out = (Bytef *)outChunk;
    strm->avail_out = sizeof(outChunk);


    while (strm->avail_in > 0)
    {
        ulong n_compressed = compressStream(strm, eof);

        if (n_compressed > sizeof(outChunk))
        {
            return -1;
        }
        strm->avail_out = sizeof(outChunk);

        int ret = sendChunk(connfd, outChunk, n_compressed);
        if (ret < 0)
        {
            return ret;
        }
    }
    return 0;
}

int sendBodyGETChunkedCompressed(z_stream *strm,
                                 FILE *file,
                                 HTTPResponse *response,
                                 FileSystemHandler *handler,
                                 int connfd)
{
    char chunk[FS_CHUNK_SIZE];
    char outChunk[FS_CHUNK_SIZE];
    int n_read = 0;
    int isEOF = 0;

    int ret = 0;

    while (!isEOF)
    {
        n_read = readFromFile(file, chunk, sizeof(chunk));
        if (n_read < 0)
        {
            ret = n_read;
            goto end_code;
        }
        ret = sendChunkCompressed(strm, chunk, n_read, connfd, (bool)feof(file));
        if (ret)
        {
            goto end_code;
        }
    }
end_code:
    sendFinalChunk(connfd);
    return ret;
}

int handleSendingBodyChunked(FILE *file, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    z_stream strm;
    int ret_prep = 0;
    int send_ret = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    switch (response->contentEncoding)
    {
    case GZIP:
        ret_prep = encode_gzip_prepare(&strm);
        break;
    case DEFLATE:
        ret_prep = encode_zlib_prepare(&strm);
        break;
    default:
        send_ret = sendBodyGETChunked(file, response, handler, connfd);
        goto check_end;
    }
    if (ret_prep)
    {
        return ret_prep;
    }
    send_ret = sendBodyGETChunkedCompressed(&strm, file, response, handler, connfd);
    deflateEnd(&strm);

check_end:
    return send_ret;
}

int readFileIntoBody(FILE *file, HTTPResponse *response,  size_t size)
{
    GrowingBuffer bodyBuffer;
    const size_t CHUNK_SIZE_BODY = 1 << 18;
    char chunk[CHUNK_SIZE_BODY];
    initGrowingBuffer(&bodyBuffer, CHUNK_SIZE_BODY);
    int ret = 0;

    while (!feof(file) && size > bodyBuffer.size )
    {
        int numRead = readFromFile(file, chunk,MIN( sizeof(chunk) , size-bodyBuffer.size ));
        if (numRead < 0)
        {
            return numRead;
        }
        appendGrowingBuffer(&bodyBuffer, chunk, numRead);
    }
    response->body = bodyBuffer.ptr;
    response->contentLength = bodyBuffer.size;
    return 0;
}

int sendChunkRangeCompressed(z_streamp strm, FILE *file, CC_Array *ranges, FileSystemHandler *handler, int connfd)
{
    Range *currentRange;

    int ret = 0;
    int queueSize = (int)cc_array_size(ranges);
    for (int i = 0; i < queueSize; i++)
    {
        enum cc_stat qstat = cc_array_get_at(ranges, i, (void **)&currentRange);
        if (qstat != CC_OK)
        {
            ret = -1;
            goto end;
        }
        int seek_ret = fseeko(file, (long)currentRange->start, SEEK_SET);
        if (seek_ret)
        {
            ret = -2;
            goto end;
        }
        char buf[currentRange->end - currentRange->start];

        assert(currentRange->end >= currentRange->start);

        int nread = readFromFile(file, buf, currentRange->end - currentRange->start);

        if (nread != sizeof(buf))
        {
            ret = -3;
            goto end;
        }
        ret = sendChunkCompressed(strm, buf, nread, connfd, ((bool)feof(file)) || i == queueSize - 1);
        if (ret)
        {
            break;
        }
    }

end:
    sendFinalChunk(connfd);
    return ret;
}


int sendChunksRangeGET(FILE *file, CC_ArrayIter *iter, FileSystemHandler *handler, int connfd)
{
    enum cc_stat qstat;
    Range *currentRange;

    int ret = 0;
    while ((qstat = cc_array_iter_next(iter, (void **)&currentRange)) == CC_OK)
    {
        int seek_ret = fseeko(file, (long)currentRange->start, SEEK_SET);
        if (seek_ret)
        {
            ret = -1;
            goto end;
        }
        char buf[currentRange->end - currentRange->start];
        assert(currentRange->end >= currentRange->start);
        int nread = readFromFile(file, buf, currentRange->end - currentRange->start);
        if (nread != sizeof(buf))
        {
            ret = -2;
            goto end;
        }
        ret = sendChunk(connfd, buf, sizeof(buf));
        if (ret)
        {
            break;
        }
    }

end:
    sendFinalChunk(connfd);
    return ret;
}


int readBodyRangeGet(FILE *file, CC_Array *ranges, HTTPResponse *response)
{
    GrowingBuffer bodyBuffer;
    initGrowingBuffer(&bodyBuffer, HTTP_STREAM_INIT_BUFFER);
    int ret = 0;

    CC_ArrayIter iter;
    cc_array_iter_init(&iter, ranges);

    enum cc_stat qstat;
    Range *currentRange;

    while ((qstat = cc_array_iter_next(&iter, (void **)&currentRange)) == CC_OK)
    {
        int seek_ret = fseeko(file, (long)currentRange->start, SEEK_SET);
        if (seek_ret)
        {
            return -1;
        }
        assert(currentRange->end >= currentRange->start);
        size_t expectedSize = currentRange->end-currentRange->start;
        char buf[expectedSize];
        const size_t CHUNK_SIZE_BODY = 1 << 15;
        int nTotal = 0;
        while (!feof(file) && nTotal < expectedSize)
        {
            int numRead = readFromFile(file, buf+nTotal, MIN(expectedSize-nTotal, CHUNK_SIZE_BODY));
            if (numRead < 0)
            {
                return numRead;
            }
            nTotal += numRead;
        }
        assert(nTotal == expectedSize);
        appendGrowingBuffer(&bodyBuffer, buf, nTotal );
    }
    response->body = bodyBuffer.ptr;
    response->contentLength = bodyBuffer.size;

    return 0;
}

int sendResponseRangeChunked(FILE *file,
                             CC_Array *ranges,
                             HTTPResponse *response,
                             FileSystemHandler *handler,
                             int connfd)
{
    int ret = 0;
    z_stream strm;


    GrowingBuffer buffer;
    initGrowingBuffer(&buffer, HTTP_STREAM_INIT_BUFFER);
    prepareHTTPResponseStatusLine(response, &buffer);
    prepareHTTPResponseMetadata(response);
    encodeHeadersResponse(response, &buffer);
    ret = sendDataTCP(connfd, buffer.ptr, buffer.size);
    if (ret)
    {
        return ret;
    }

    switch (response->contentEncoding)
    {
    case GZIP:
        ret = encode_gzip_prepare(&strm);
        break;
    case DEFLATE:
        ret = encode_zlib_prepare(&strm);
        break;
    default:
        CC_ArrayIter iter;
        cc_array_iter_init(&iter, ranges);
        ret = sendChunksRangeGET(file, &iter, handler, connfd);
        return ret;
    }
    if (ret)
    {
        return ret;
    }
    ret = sendChunkRangeCompressed(&strm, file, ranges, handler, connfd);
    return ret;
}


int handleHEADFile(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{

    Path *path = &request->uriPath;
    char pathStr[PATH_MAX];
    pathToStr(path, pathStr);

    if (access(pathStr, F_OK) == -1)
    {
        makeNotFound(response, "Not found!");
        return -1;
    }

    struct stat file_stat;

    int stat_res = stat(pathStr, &file_stat);
    if (stat_res)
    {
        makeServerError(response, "Error analyzing file");
        return -1;
    }

    bool isDir = S_ISDIR(file_stat.st_mode);

    if (!isDir)
    {
        setHeader(response, ACCEPT_RANGE_HEADER_NAME, "bytes");
        char *mimeStr = getMimeTypeForFile(pathStr);
        setHeader(response, CONTENT_TYPE_HEADER_NAME, mimeStr);
        response->contentLength = (int)file_stat.st_size;
    }
    else
    {
        response->contentLength = 0;
    }
    sendResponse(response, connfd);

    return 0;
}

int handleGETRanged(FILE *openedFile, struct stat * file_stat, HTTPRequest *request, HTTPResponse *response, char *rangeStr)
{
    int ret = 0;
    CC_Array *listRanges;
    initCCArr(&listRanges);


    ret = decodeRanges(rangeStr, file_stat, listRanges);
    if (ret)
    {
        makeRangeUnSatisfiable(response,"Range not satisfiable", file_stat->st_size);
        return ret;
    }
    Range *rangeInitial = NULL;
    cc_array_get_at(listRanges, 0, (void **)&rangeInitial);
    const size_t MAX_REQ_SIZE = 1 << 22;
    if (rangeInitial->end - rangeInitial->start >= MAX_REQ_SIZE)
    {
        rangeInitial->end = rangeInitial->start + MAX_REQ_SIZE;
    }
    int seek_ret = fseeko(openedFile, (long)rangeInitial->start, SEEK_SET);
    size_t offset = ftello(openedFile);
    assert(offset == rangeInitial->start);
    if (seek_ret)
    {
        return -2;
    }

    ret = readFileIntoBody(openedFile, response, rangeInitial->end-rangeInitial->start+1);
    if (ret)
    {
        makeServerError(response, "Unable to read the file!");
        return ret;
    }
    response->statusCode = HTTP_PARTIAL_CONTENT;

    char contentRangeS[128];
    snprintf(contentRangeS,sizeof(contentRangeS), "bytes %ld-%ld/%ld", rangeInitial->start, rangeInitial->end, file_stat->st_size);
    setHeader(response, CONTENT_RANGE_HEADER_NAME, contentRangeS);

    return 0;
}

int handleGETFile(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{


    FILE *openedFile = NULL;
    Path *path = &request->uriPath;
    char pathStr[PATH_MAX];
    int ret = 0;
    do
    {
        pathToStr(path, pathStr);
        openedFile = fopen(pathStr, "r+");
        if (openedFile == NULL)
        {
            bool tryAgain = false;
            //
            switch (errno)
            {
            case ENOTDIR:
            case EPERM:
                makeNotFound(response, "Server has insufficient permission to read the file!");
                ret = -1;
                goto error;
            case EISDIR:
                char *valueList = getQueryParam(request, "list");
                if (valueList)
                {
                    ret = handleDirectoryList(pathStr, request, response, handler, connfd);
                    goto closefile;
                }

                tryAgain = true;
                handleDirectory(path, request, response, handler);
                break;
            case EINTR:
                // process was just handling a signal, unlikely to happen
                tryAgain = true;
                break;
            default:
                break;
            }
            if (!tryAgain)
            {
                break;
            }
        }
    } while (openedFile == NULL);

    if (!openedFile)
    {
        // still null, it failed, send response back
        handleNotFound(path, request, response, handler);
        ret = -1;
        goto error;
    }
    // test if file exists
    if (access(pathStr, F_OK) == -1)
    {
        ret = -1;
        makeNotFound(response, "File doesn't exist!");
        goto error;
    }

    if (chooseContentType(pathStr, request, response, connfd))
    {

        ret = -1;
        makeMediaTypeNotSupported(response, "Media Type not supported for this file!");
        goto error;
    }
    setHeader(response, ACCEPT_RANGE_HEADER_NAME, "bytes");

    struct stat fstat;
    stat(pathStr, &fstat);

    char *rangeHeader = getHeader(request, RANGE_HEADER_NAME);
    if (rangeHeader )
    {
        if ((ret = handleGETRanged(openedFile,&fstat,  request, response, rangeHeader))){
            goto error;
        }

        goto send_resp;
    }

    ret = readFileIntoBody(openedFile, response,  fstat.st_size );
    if (ret)
    {
        makeServerError(response, "Failed to read file!");
        goto error;
    }

send_resp:
    sendResponse(response, connfd);

closefile:
    if (openedFile)
    {
        fclose(openedFile);
    }
    return ret;
error:
    // any error cleanup
    // the request handler handles sending error responses if need be
    goto closefile;
}

void replacePrefixWithWebroot(HTTPRequest *request, FileSystemHandler *fsHandler)
{
    sanitizePath(&request->uriPath);

    removePrefix(&request->uriPath, &fsHandler->pathPrefix);

    // add webroot to prefix the path
    concatenatePath(&fsHandler->webroot, &request->uriPath);
}


int FSHandlerCallbackPublic(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    replacePrefixWithWebroot(request, handler);
    switch (request->method)
    {
    case GET:
        return handleGETFile(request, response, handler, connfd);
    case HEAD:
        return handleHEADFile(request, response, handler, connfd);
    default:
        makeMethodNotSupported(response, NULL);
        break;
    }
    return -1;
}
int FSHandlerCallbackPrivate(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    replacePrefixWithWebroot(request, handler);
    switch (request->method)
    {
    case DELETE:
        return handleDELETEFile(request, response, handler, connfd);
    case POST:
        return handlePOSTFile(request, response, handler, connfd);
    default:
        response->statusCode = HTTP_METHOD_UNSUPPORTED;
        return -1;
    }
}

Route getPublicFSHandlerObj(FileSystemHandler *fsHandler)
{
    enum http_method methods[2] = {GET, HEAD};
    Route route;
    initRoute(&route, fsHandler->pathPrefix, methods, sizeof(methods) / sizeof(enum http_method));
    route.handlerObject = fsHandler;
    route.handleCallback = FSHandlerCallbackPublic;
    return route;
}

Route getPrivateFSHandlerObj(FileSystemHandler *fsHandler)
{
    enum http_method methods[2] = {POST, DELETE};
    Route route;
    initRoute(&route, fsHandler->pathPrefix, methods, sizeof(methods) / sizeof(enum http_method));
    route.handlerObject = fsHandler;
    route.handleCallback = FSHandlerCallbackPrivate;
    addFilterToChain(&route, csrfFilter, NULL);
    addFilterToChain(&route, authFilter, NULL);
    return route;
}

void initFileSystemHandler(FileSystemHandler *fsHandler, char *pathPrefix, char *webroot)
{

    stringToPath(&fsHandler->pathPrefix, pathPrefix);
    stringToPath(&fsHandler->webroot, webroot);
    fsHandler->acceptedEncodings = NULL;
}

void addFileSystemHandler(Router *router, FileSystemHandler *fsHandler)
{
    Route route = getPrivateFSHandlerObj(fsHandler);
    addRoute(router, &route);
    route = getPublicFSHandlerObj(fsHandler);
    addRoute(router, &route);
}
