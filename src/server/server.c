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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CONN 5

void initServer(Server **server_p, char *host, char *name, char *webroot, int maxWorkers)
{

    Server *server = malloc(sizeof(Server));
    *server_p = server;
    cc_dynamic_pool_new(8128, &server->arena);

    copyStringToPool(&server->host, host, server->arena);
    copyStringToPool(&server->name, name, server->arena);
    copyStringToPool(&server->webroot, webroot, server->arena);

    server->port = -1;
    server->maxWorkers = maxWorkers;
    initThreadPool(&server->pool, maxWorkers, server->arena);
    server->ipAddr = NULL;
    server->socketfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server->sock, 0, sizeof(struct sockaddr_in));
    initRouter(&server->router, server->arena);
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
    cc_dynamic_pool_destroy(server->arena);
}

void setServerAddress(Server *server, char *ipAddr, int port)
{
    server->port = port;
    copyStringToPool(&server->ipAddr, ipAddr, server->arena);
    struct sockaddr_in server_add;
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
        CC_DynamicPoolConf poolConf;
        CC_DynamicPool* pool;
        cc_dynamic_pool_conf_init(&poolConf);
        poolConf.exp_factor = 2;
        poolConf.is_fixed = false;
        cc_dynamic_pool_new_conf(HTTP_POOL_INITIAL_SIZE, &poolConf, &pool );

        HTTPRequest request;
        initHTTPRequest(&request,pool );

        HTTPResponse response;
        initHTTPResponse(&response, pool);

        int result = scanRequest(connfd, &request, &keepAlive, &statusCode);
        response.statusCode = statusCode;
        setRequest(&response, &request);


        if (result == -1)
        {
            printf("%d", statusCode);
            goto send_resp;
        }
        RequestHandler *handler = longestPrefixMatch(&server->router, &request);
        if (!handler){
            // 404 no handler found

            const char * resp = "404 Not found!";
            response.statusCode = HTTP_NOT_FOUND;
            setResponseBody(&response, resp, strlen(resp));
            goto send_resp;
        }
        handler->handleCallback(&request, &response, handler->handlerObject);


        send_resp:
            sendResponse(&response, connfd);
            goto free_code;

        free_code:
            freeHTTPRequest(&request);
            freeHTTPResponse(&response);
            cc_dynamic_pool_destroy(pool);
    }
}


void initRouter(Router *router, CC_DynamicPool *pool)
{
    router->pool = pool;
    CC_ArrayConf conf;
    cc_array_conf_init(&conf);
    conf.pool = pool;
    cc_array_new_conf(&conf, (&router->requestHandlers));
}

void addHandler(Router *router, RequestHandler *handler)
{
    RequestHandler *newPtr = cc_dynamic_pool_malloc(sizeof(RequestHandler), router->pool);
    memcpy(newPtr, handler, sizeof(RequestHandler));
    cc_array_add(router->requestHandlers, newPtr);
}
RequestHandler *longestPrefixMatch(Router *router, HTTPRequest *request)
{
    int maxMatch = 0;
    RequestHandler *longestMatchHandler = NULL;

    CC_ArrayIter iterator;
    cc_array_iter_init(&iterator, router->requestHandlers);
    RequestHandler *currentHandler;
    enum cc_stat stat;
    while ((stat = cc_array_iter_next(&iterator, (void **)&currentHandler)) != CC_ITER_END)
    {
        int match = currentHandler->getPrefixMatch(request, currentHandler->handlerObject);
        if (match > maxMatch)
        {
            maxMatch = match;
            longestMatchHandler = currentHandler;
        }
    }
    return longestMatchHandler;
}
void freeRouter(Router *router)
{
    // cc_array_destroy(router->requestHandlers);
}


void shutDownServer(Server *server)
{
    shutDownThreadPool(&server->pool);
    freeServer(server);
}
