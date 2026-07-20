#include "cc_deque.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "utils.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <server.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FS_CHUNK_SIZE 64

void sanitizeURI(char *uri, char*output)
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


int prefixMatchPaths(Path* path1, Path* path2, CC_DynamicPool* pool)
{
    Path common;
    commonRoot(&common, path1, path2);

    return (int)cc_deque_size(common.directories) ;
}

int prefixMatchFSHandler(HTTPRequest *request, void *handler)
{
    FileSystemHandler *fsHandler = (FileSystemHandler *)handler;
    return prefixMatchPaths(&fsHandler->pathPrefix, &request->uriPath, request->pool);
}

int handleDirectory(Path* fullPath, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{
    // append
    addToPath(fullPath, "index.html");
    return 0;
}

int handleNotFound(Path* fullPath, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{
    response->statusCode = HTTP_NOT_FOUND;
    const char * respStr = "Not Found!";
    char * ptr = cc_dynamic_pool_malloc( strlen(respStr)+1, response->pool);
    strcpy(ptr, respStr);
    response->body = ptr ;
    response->contentLength = (int)strlen(respStr);
    return -1;
}

int tryFiles(Path* path, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler)
{

    FILE *openedFile = NULL;
    char pathStr[PATH_MAX];
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
            case ENOTDIR : case EPERM:
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
            if (!tryAgain){
                break;
            }
        }
    } while (openedFile == NULL);

    if (!openedFile){
        // still null, it failed, send response back
        handleNotFound(path, request, response, handler);
        return -1;
    }
    // test if file exists
    if ( access(pathStr, R_OK) == -1){
        // file doesnt exist or we cant read it
        handleNotFound(path, request, response, handler);
        goto closeFile;
    }

    // read from the file
    // write contents to body

    GrowingBuffer buffer ;
    initGrowingBuffer(&buffer, response->pool, FS_CHUNK_SIZE<<1);
    char chunk [FS_CHUNK_SIZE];
    size_t numRead = 0;
    while (!feof(openedFile) && ((numRead =  fread(chunk,sizeof(char), sizeof(chunk), openedFile)) != 0)){
        appendGrowingBuffer(&buffer, chunk, numRead );
    }
    setResponseBody(response, buffer.ptr, buffer.size);

    // file is sucessfully found

    closeFile:
        fclose(openedFile);
        return 0;
}

int FSHandlerCallback(HTTPRequest *request, HTTPResponse *response, void *handler)
{
    FileSystemHandler *fsHandler = (FileSystemHandler *)handler;

    sanitizePath(&request->uriPath);

    removePrefix(&request->uriPath, &fsHandler->pathPrefix);

    // add webroot to prefix the path
    concatenatePath(&fsHandler->webroot, &request->uriPath);

    return tryFiles(&request->uriPath, request, response, fsHandler);
}


RequestHandler getFSHandlerObj(FileSystemHandler *fsHandler)
{
    return (RequestHandler){
        .handlerObject = fsHandler,
        .getPrefixMatch = prefixMatchFSHandler,
        .handleCallback = FSHandlerCallback,
    };
}

void initFileSystemHandler(FileSystemHandler* fsHandler, char * pathPrefix, char* webroot, CC_DynamicPool* pool){

    stringToPath(&fsHandler->pathPrefix, pathPrefix, pool);
    stringToPath(&fsHandler->webroot, webroot, pool);
    fsHandler->acceptedEncodings  = NULL;
}

void addFileSystemHandler(Router* router, FileSystemHandler* fsHandler){
    RequestHandler handler = getFSHandlerObj(fsHandler);
    addHandler(router, &handler);
}
