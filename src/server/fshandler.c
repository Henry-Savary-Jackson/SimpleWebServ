#include "cc_deque.h"
#include "cc_pqueue.h"
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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
    switch (request->method) {
        case GET:
            addToPath(fullPath, "index.html");
            return  0;
        default:
            response->statusCode  =HTTP_FORBIDDEN;
            const char * msg = "Is a directory!";
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
    response->body = custom_strdup(respStr);
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
        assert(encodingPqueue);
        response->contentEncoding = decideContentEncoding(encodingPqueue, &response->contentEncoding);
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

int handlePOSTFile(Path *path,char* pathStr, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
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
                tryAgain = handleDirectory(path, request, response, handler) == 0;
                break;
            case EINTR:
                // process was just handling a signal, unlikely to happen
                tryAgain = true;
                break;
            default:
                break;
            }
        }
    } while (openedFile ==NULL);
    if (!openedFile)
    {
        // still null, it failed, send response back
        response->statusCode = HTTP_SERVER_ERROR;
        ret = -1;
        goto closefile;
    }

    size_t nwritten = fwrite( request->body, 1, request->contentLength, openedFile);
    if (nwritten < 0){
        // errro
        response->statusCode = HTTP_SERVER_ERROR;
        ret = -1;
        goto closefile;
    }

    closefile:
        fclose(openedFile);
        return ret;
}

int handleGETFile(Path *path, char* pathStr, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
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

    GrowingBuffer buffer;
    initGrowingBuffer(&buffer, FS_CHUNK_SIZE << 1);
    char chunk[FS_CHUNK_SIZE];
    size_t numRead = 0;
    while (!feof(openedFile) && ((numRead = fread(chunk, sizeof(char), sizeof(chunk), openedFile)) != 0))
    {
        appendGrowingBuffer(&buffer, chunk, numRead);
    }
    setResponseBody(response, buffer.ptr, buffer.size);

closeFile:
    fclose(openedFile);
    return ret;
}


int tryFiles(Path *path, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd)
{
    char pathStr[PATH_MAX];
    int ret = 0;
    switch (request->method){
        case GET:
            ret =handleGETFile(path,pathStr, request, response,handler, connfd);
            break;
        case POST:
            ret = handlePOSTFile(path,pathStr, request, response,handler, connfd);
            break;
        default:
            response->statusCode = HTTP_METHOD_UNSUPPORTED;
            break;
    }


    // check if there are not

    if (chooseContentType(pathStr, request, response, handler, connfd))
    {
        return -1;
    }

    if (chooseContentEncoding(pathStr, request, response, handler, connfd))
    {
        return -1;
    }

    // write contents to body

    response->statusCode = HTTP_OK;
    sendResponse(response, connfd);
    // file is sucessfully found

    return 0;
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
