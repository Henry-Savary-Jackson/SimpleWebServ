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
        makeNotFound(response);
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
        makeNotFound(response);
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
        makeBadRequest(response);
        goto finish;
    }

    if (request->contentLength == 0)
    {
        unlink(pathStr);
        ret = mkdir(pathStr, 7);
        goto finish;
    }

    ret = handleRequestContentEncoding(request, response, handler, connfd);

    if (ret)
    {
        makeBadRequest(response);
        goto finish;
    }
    // if you didnt using chunked, then start writing the final body
    ret = writeToFile(openedFile, request->body, request->contentLength);

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

int sendBodyGETChunkedCompressed(z_stream *strm,
                                 FILE *file,
                                 HTTPResponse *response,
                                 FileSystemHandler *handler,
                                 int connfd)
{
    char chunk[FS_CHUNK_SIZE];
    char outChunk[FS_CHUNK_SIZE];
    int n_read = 0;
    int n_total_read = 0;
    int total_size = 0;
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
        isEOF = feof(file);

        strm->avail_in = n_read;
        n_total_read += n_read;
        while (strm->avail_in > 0)
        {
            int n_compressed = compressChunk(strm, chunk, n_read, outChunk, sizeof(outChunk), (bool)isEOF);
            if (n_compressed < 0)
            {
                ret = n_compressed;
                goto end_code;
            }
            int ret = sendChunk(connfd, outChunk, n_compressed);
            total_size += n_compressed;
            if (ret < 0)
            {
                goto end_code;
            }
        }
    }
end_code:
    sendFinalChunk(connfd);
    return 0;
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

int readFileIntoBody(FILE *file, HTTPResponse *response, FileSystemHandler *handler)
{
    GrowingBuffer bodyBuffer;
    char chunk[FS_CHUNK_SIZE];
    initGrowingBuffer(&bodyBuffer, FS_CHUNK_SIZE);
    int ret = 0;

    while (!feof(file))
    {
        int numRead = readFromFile(file, chunk, sizeof(chunk));
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

int sendBodyGET(FILE *file, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    GrowingBuffer outBuffer;
    initGrowingBuffer(&outBuffer, HTTP_STREAM_INIT_BUFFER);

    int ret = 0;

    prepareHTTPResponseStatusLine(response, &outBuffer);

    // if gzip
    switch (response->transferEncoding)
    {
    case CHUNKED:
        prepareHTTPResponseMetadata(response);
        encodeHeaders(response, &outBuffer);
        sendDataTCP(connfd, outBuffer.ptr, outBuffer.size);
        return handleSendingBodyChunked(file, response, handler, connfd);
    case GZIP:
    case DEFLATE:
        readFileIntoBody(file, response, handler);
        prepareResponseBody(response);
        compressReponseBody(response, response->transferEncoding);
        break;
    default:
        readFileIntoBody(file, response, handler);
        prepareResponseBody(response);
        break;
    }

    prepareHTTPResponseMetadata(response);
    encodeHeaders(response, &outBuffer);
    encodeResponseBody(response, &outBuffer);

    return sendDataTCP(connfd, outBuffer.ptr, outBuffer.size);
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
                makeNotFound(response);
                ret = -1;
                goto error;
            case EISDIR:
                tryAgain = handleDirectory(path, request, response, handler) == 0;
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
        makeNotFound(response);
        goto error;
    }

    if (chooseContentType(pathStr, request, response, connfd))
    {

        ret = -1;
        makeMediaTypeNotSupported(response);
        goto error;
    }
    ret = sendBodyGET(openedFile, response, handler, connfd);

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
    return handleGETFile(request, response, handler, connfd);
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
    enum http_method methods[1] = {GET};
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
    cc_array_add(route.filterChain, &csrfFilter);
    cc_array_add(route.filterChain, &authFilter);
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
