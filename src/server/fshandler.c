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


int prefixMatchPaths(Path *path1, Path *path2)
{
    Path common;
    commonRoot(&common, path1, path2);

    return (int)cc_deque_size(common.directories);
}

int prefixMatchFSHandler(HTTPRequest *request, void *handler)
{
    FileSystemHandler *fsHandler = (FileSystemHandler *)handler;
    return prefixMatchPaths(&fsHandler->pathPrefix, &request->uriPath);
}

int handleDirectory(Path *fullPath, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{
    switch (request->method)
    {
    case GET:
        addToPath(fullPath, "index.html");
        return 0;
    default:
        response->statusCode = HTTP_FORBIDDEN;
        const char *msg = "Is a directory!";
        setResponseBody(response, msg, strlen(msg));
        return -1;
    }
    // append
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

int chooseContentType(char *path, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    // check if mimetype desire matches
    // check if
    CC_Deque *acceptMimetypes = getHeaderValues(request->headers, ACCEPT_MIMETYPE_HEADER_NAME);
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

int chooseContentEncoding(char *path,
                          HTTPRequest *request,
                          HTTPResponse *response,
                          FileSystemHandler *handler,
                          int connfd)
{
    CC_Deque *acceptEncodings = getHeaderValues(request->headers, ACCEPT_ENCODING_HEADER_NAME);
    if (acceptEncodings != NULL)
    {
        CC_PQueue *encodingPqueue = decodeAcceptEncodings(acceptEncodings);
        if (!encodingPqueue)
        {
            return -1;
        }
        int result = decideContentEncoding(encodingPqueue, &response->contentEncoding);
        if (response->contentEncoding == UNKNOWN_ENCODING)
        {
            response->statusCode = HTTP_NOT_ACCEPTED;
            response->contentType = "text/html";
            response->contentEncoding = IDENTITY_ENCODING;
            sendResponse(response, connfd);
            return -1;
        }
    }
    else
    {
        response->contentEncoding = IDENTITY_ENCODING;
    }
    return 0;
}

int chooseServerTransferCoding(HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char *transferCodingStr = getHeader(request->headers, TRANSFER_CODING_HEADER_NAME);
    if (!transferCodingStr)
    {
        return 0;
    }
    request->transferEncoding = HTTP_ENCODING(transferCodingStr);
    if (request->transferEncoding == UNKNOWN_ENCODING)
    {
        response->statusCode = HTTP_NOT_ACCEPTED;
        return -1;
    }
    return 0;
}

int chooseClientTransferCoding(char *path,
                               HTTPRequest *request,
                               HTTPResponse *response,
                               FileSystemHandler *handler,
                               int connfd)
{

    CC_Deque *acceptTE = getHeaderValues(request->headers, TRANSFER_ENCODING_CLIENT_HEADER_NAME);
    if (acceptTE != NULL)
    {
        CC_PQueue *encodingPqueue = decodeAcceptEncodings(acceptTE);
        if (!encodingPqueue)
        {
            return -1;
        }
        if (decideTransferEncoding(encodingPqueue, &response->transferEncoding))
        {
            response->statusCode = HTTP_NOT_ACCEPTED;
            response->contentType = "text/html";
            response->contentEncoding = IDENTITY_ENCODING;
            response->transferEncoding = IDENTITY_ENCODING;
            sendResponse(response, connfd);
            return -1;
        }
    }
    return 0;
}

int writeToFile(FILE *file, char *chunk, int size)
{
    size_t totalWritten = 0;
    size_t nwritten = 0;
    while (totalWritten < size)
    {
        nwritten = fwrite(chunk + totalWritten, 1, size - totalWritten, file);
        if (ferror(file) && errno == EINTR)
        {
            clearerr(file);
        }
        if (ferror(file))
        {
            return -1;
        }
        totalWritten += nwritten;
    }
    return 0;
}

int readFromFile(FILE *file, char *chunk, int size)
{
    size_t totalRead = 0;
    size_t numRead = 0;
    while (totalRead < size)
    {
        numRead = fread(chunk + totalRead, 1, size - totalRead, file);
        if (feof(file))
        {
            return (int)(totalRead + numRead);
        }
        if (ferror(file) && errno == EINTR)
        {
            clearerr(file);
        }
        else if (ferror(file))
        {
            return -1;
        }
        totalRead += numRead;
    }
    return (int)totalRead;
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
    int ret = 0;
    switch (request->transferEncoding)
    {
    case CHUNKED:
        return handleChunkedTransferCoding(file, request, handler, connfd);
    default:
        ret = scanBody(request);
        if (ret)
        {
            return ret;
        }
        ret = decompressBody(request, request->transferEncoding);
        break;
    }
    return ret;
}

int handleDELETEFile(Path *path,
                   HTTPRequest *request,
                   HTTPResponse *response,
                   FileSystemHandler *handler,
                   int connfd)
{
    char pathStr[PATH_MAX];
    pathToStr(path, pathStr);
    if (!access(pathStr, F_OK)){

        struct stat stat_res;

        stat(pathStr,&stat_res );

        bool isDir = S_ISDIR(stat_res.st_mode);
        if (isDir){
            return rmdir(pathStr);
        }
        return unlink(pathStr);
    }
    response->statusCode = HTTP_NOT_FOUND;
    return -1;
}


int handleRequestContentEncoding(HTTPRequest* request, HTTPResponse* response, FileSystemHandler* handler, int connfd){
    char* newBodyPtr = NULL;
    int newSize = 0;
    int ret = 0;
    switch (request->contentEncoding){
        case GZIP:
            ret = decode_gzip(request->body , request->contentLength, &newBodyPtr, &newSize);
            if (ret){
                return ret;
            }
            break;
        case DEFLATE:
            ret = decode_zlib(request->body , request->contentLength, &newBodyPtr, &newSize);
            if (ret){
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

int handlePOSTFile(Path *path,
                   char *pathStr,
                   HTTPRequest *request,
                   HTTPResponse *response,
                   FileSystemHandler *handler,
                   int connfd)
{
    FILE *openedFile = NULL;
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
            case EISDIR:
                ret = mkdir(pathStr, 7);
                if (ret){
                    goto closefile;
                }
                break;
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
        response->statusCode = HTTP_SERVER_ERROR;
        ret = -1;
        goto closefile;
    }

    if (chooseServerTransferCoding(request, response, handler, connfd))
    {
        ret = -1;
        goto closefile;
    }

    ret = handleTransferCodingRequest(openedFile, request, response, handler, connfd);

    if (ret == 0)
    {
        ret = handleRequestContentEncoding(request, response, handler, connfd);
        if (ret){
            goto closefile;
        }
        if (request->contentLength ==0){
            goto makedir;
        }
        // if you didnt using chunked, then start writing the final body
        ret = writeToFile(openedFile, request->body, request->contentLength);
    }
    if (ret > 0)
    {
        // you chunked the writing to the file
        ret = 0;
    }

closefile:
    fclose(openedFile);
    return ret;

makedir:
    unlink(pathStr);
    return mkdir(pathStr, 7);
}

int sendBodyGETChunked(FILE *file, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char chunk[FS_CHUNK_SIZE];
    int n_read = 0;
    while (!feof(file))
    {

        n_read = readFromFile(file, chunk, sizeof(chunk));
        int end = feof(file);
        if (n_read < 0)
        {
            return n_read;
        }
        int sendRet = sendChunk(connfd, chunk, n_read);
        if (sendRet < 0)
        {
            return sendRet;
        }
    }
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
    while (!isEOF)
    {
        n_read = readFromFile(file, chunk, sizeof(chunk));
        if (n_read < 0)
        {
            return n_read;
        }
        isEOF = feof(file);

        strm->avail_in = n_read;
        n_total_read += n_read;
        while (strm->avail_in > 0)
        {
            int n_compressed = compressChunk(strm, chunk, n_read, outChunk, sizeof(outChunk), (bool)isEOF);
            if (n_compressed < 0)
            {
                return n_compressed;
            }
            int sendRet = sendChunk(connfd, outChunk, n_compressed);
            total_size += n_compressed;
            if (sendRet < 0)
            {
                return sendRet;
            }
        }
    }
    sendFinalChunk(connfd);
    return 0;
}

int handleSendingBodyChunked(FILE *file, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    z_stream strm;
    int ret_prep = 0;
    int send_res = 0;
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
        send_res = sendBodyGETChunked(file, response, handler, connfd);
        goto check_end;
    }
    if (ret_prep)
    {
        return ret_prep;
    }
    send_res = sendBodyGETChunkedCompressed(&strm, file, response, handler, connfd);
    deflateEnd(&strm);

check_end:
    if (send_res)
    {
        return send_res;
    }
    return 0;
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
    ;
}

int handleGETFile(Path *path,
                  char *pathStr,
                  HTTPRequest *request,
                  HTTPResponse *response,
                  FileSystemHandler *handler,
                  int connfd)
{
    FILE *openedFile = NULL;
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
                handleNotFound(path, request, response, handler);
                break;
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
        return -1;
    }
    // test if file exists
    if (access(pathStr, R_OK) == -1)
    {
        // file doesnt exist or we cant read it
        handleNotFound(path, request, response, handler);
        ret = -1;
        goto closeFile;
    }

    if (chooseContentType(pathStr, request, response, handler, connfd))
    {
        ret = -1;
        goto closeFile;
    }

    if (chooseContentEncoding(pathStr, request, response, handler, connfd))
    {
        ret = -1;
        goto closeFile;
    }

    if (chooseClientTransferCoding(pathStr, request, response, handler, connfd))
    {
        ret = -1;
        goto closeFile;
    }

    ret = sendBodyGET(openedFile, response, handler, connfd);
    // sendBody

closeFile:
    fclose(openedFile);
    return ret;
}


int tryFiles(Path *path, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char pathStr[PATH_MAX];
    int ret = 0;
    switch (request->method)
    {
    case GET:
        return handleGETFile(path, pathStr, request, response, handler, connfd);
    case POST:
        ret = handlePOSTFile(path, pathStr, request, response, handler, connfd);
        break;
    case DELETE:
        ret = handleDELETEFile(path, request, response, handler, connfd);
        break;
    default:
        response->statusCode = HTTP_METHOD_UNSUPPORTED;
        break;
    }
    response->version = "1.1";
    sendResponse(response, connfd);
    return ret;
}

int FSHandlerCallback(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    FileSystemHandler *fsHandler = (FileSystemHandler *)handler;

    sanitizePath(&request->uriPath);

    removePrefix(&request->uriPath, &fsHandler->pathPrefix);

    // add webroot to prefix the path
    concatenatePath(&fsHandler->webroot, &request->uriPath);

    return tryFiles(&request->uriPath, request, response, fsHandler, connfd);
}


RequestHandler getFSHandlerObj(FileSystemHandler *fsHandler)
{
    return (RequestHandler){
        .handlerObject = fsHandler,
        .getPrefixMatch = prefixMatchFSHandler,
        .handleCallback = FSHandlerCallback,
    };
}

void initFileSystemHandler(FileSystemHandler *fsHandler, char *pathPrefix, char *webroot)
{

    stringToPath(&fsHandler->pathPrefix, pathPrefix);
    stringToPath(&fsHandler->webroot, webroot);
    fsHandler->acceptedEncodings = NULL;
}

void addFileSystemHandler(Router *router, FileSystemHandler *fsHandler)
{
    RequestHandler handler = getFSHandlerObj(fsHandler);
    addHandler(router, &handler);
}
