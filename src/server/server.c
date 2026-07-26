#include "server.h"
#include "cc_array.h"
#include "cc_common.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "threadpool.h"
#include "utils.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <parser.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <unistd.h>

#define MAX_CONN 5

thread_local Arena arena;

void* custom_alloc(size_t size){
    return arena_alloc(&arena, size);
}
void* custom_calloc(size_t blocks,size_t size){
    void * ptr =custom_alloc( blocks*size);
    memset(ptr,0,blocks*size);
    return ptr;
}
void custom_free(void* ptr){
    arena_trim(&arena);
}
char* custom_strdup(char* str){
    return arena_strdup(&arena, str);
}

void initServer(Server **server_p, char *host, char *name, char *webroot, int maxWorkers)
{

    Server *server = malloc(sizeof(Server));
    *server_p = server;

    server->host = arena_strdup(&arena, host);
    server->name= arena_strdup(&arena, name);
    server->webroot= arena_strdup(&arena, webroot);

    server->port = -1;
    server->maxWorkers = maxWorkers;
    initThreadPool(&server->pool, maxWorkers);
    server->ipAddr = NULL;
    server->socketfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server->sock, 0, sizeof(struct sockaddr_in));
    initRouter(&server->router);
}

void addFileSystemHandlerServer(Server *server, FileSystemHandler *handler)
{
    addFileSystemHandler(&server->router, handler);
}

void freeServer(Server *server)
{
    if (server->socketfd)
    {
        close(server->socketfd);
    }
    arena_free(&arena);
}

void setServerAddress(Server *server, char *ipAddr, int port)
{
    server->port = port;
    server->ipAddr = arena_strdup(&arena, ipAddr);
    server->sock.sin_addr.s_addr = inet_addr(ipAddr);
    server->sock.sin_port = htons(port);
    server->sock.sin_family = AF_INET;
}

int bindSocket(Server *server)
{
    // initialize
    int reuseaddr = 1;
    if (setsockopt(server->socketfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)))
    {
        perror("setsockopt failed");
        return -1;
    }

    int result = bind(server->socketfd, (struct sockaddr *)&server->sock, sizeof(server->sock));
    if (result)
    {
        if (errno == EADDRINUSE)
        {
            printf("Address already in use!\n");
        }
        printf("%d Failed to bind to port!\n", errno);
        return -1;
    }
    printf("PID: %d \nPort %d bound on address %s !\n", getpid(), server->port, server->ipAddr);
    return 0;
}
void callbackAccept(void *data)
{
    struct
    {
        int connfd;
        Server *server;
    } *args = data;
    handleConnection(args->connfd, args->server);
    free(data);
}

void launch(Server *server)
{
    int listen_result = listen(server->socketfd, MAX_CONN);
    if (listen_result)
    {
        printf("Error listening!\n");
        return;
    }
    startThreadPool(&server->pool);
    while (true)
    {
        struct sockaddr_in cli;

        int len = sizeof(cli);

        int connfd = accept(server->socketfd, (struct sockaddr *)&cli, (socklen_t *)&len);

        if (connfd < 0)
        {
            if (errno && errno != EINTR)
            {
                printf("\npid:%d Server accept failed!\n", getpid());
                return;
            }
            continue;
        }
        // link thread pool server nad socket fd to new thread
        typedef struct
        {
            int connfd;
            Server *server;
        } callback_args;
        callback_args *args = malloc(sizeof(callback_args));
        args->connfd = connfd;
        args->server = server;
        submitTask(&server->pool, callbackAccept, args);
    }
}

void handleConnection(int connfd, Server *server)
{
    bool keepAlive = true;
    int statusCode = HTTP_OK;
    while (keepAlive)
    {
        HTTPRequest request;
        initHTTPRequest(&request );

        HTTPResponse response;
        initHTTPResponse(&response);

        HTTPStream inStream;
        initHTTPStream(&inStream, connfd);
        request.inputStream = &inStream;

        int result = scanFirstLine( &request);
        if (result == -1){
            statusCode = HTTP_BAD_REQUEST;
            goto send_error_resp;
        }

        result = scanHeaders(&request);

        if (result == -1){
            statusCode = HTTP_BAD_REQUEST;
            goto send_error_resp;
        }

        result = prepareHTTPRequestMetadata(&request,&statusCode, &keepAlive  );

        if (result == -1)
        {
            printf("%d", statusCode);
            statusCode = HTTP_BAD_REQUEST;
            goto send_error_resp;
        }

        setRequest(&response, &request);

        // choose a request handler that matches the url endpoint the most
        RequestHandler *handler = longestPrefixMatch(&server->router, &request);
        if (!handler){
            // 404 if no handler found

            const char * resp = "404 Not found!";
            statusCode = HTTP_NOT_FOUND;
            setResponseBody(&response, resp, strlen(resp));
            goto send_error_resp;
        }
        // if a handler is found call the handler function that will decide how to mofidy the request
        // and send data back via TCP to the client
        handler->handleCallback(&request, &response, handler->handlerObject, connfd);

        // free the http request and the associated meory in the arena
        // no need to deallocate, keep the meory for future requests
        free_code:
            freeHTTPRequest(&request);
            freeHTTPResponse(&response);
            arena_reset(&arena);
            return ;
        send_error_resp:
            response.statusCode = statusCode;
            sendResponse(&response, connfd);
            goto free_code;
    }
}


void shutDownServer(Server *server)
{
    shutDownThreadPool(&server->pool);
    freeServer(server);
}
