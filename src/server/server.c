#include "server.h"
#include "http.h"
#include "threadpool.h"
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

void initServer(Server *s, char *host, char *name, char *webroot, int maxWorkers)
{
    s->host = strdup(host);
    s->name = strdup(name);
    s->webroot = strdup(webroot);
    s->port = -1;
    s->maxWorkers = maxWorkers;
    initThreadPool(&s->pool, maxWorkers);
    s->ipAddr = NULL;
    s->socketfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&s->sock, sizeof(struct sockaddr_in));
}

void freeServer(Server *s)
{
    if (s->host)
        free(s->host);
    if (s->name)
        free(s->name);
    if (s->webroot)
        free(s->webroot);
    if (s->ipAddr)
        free(s->ipAddr);
    if (s->socketfd)
        close(s->socketfd);
}

void setServerAddress(Server *s, char *ipAddr, int port)
{
    s->port = port;
    if (s->ipAddr)
        free(s->ipAddr);
    s->ipAddr = strdup(ipAddr);
    struct sockaddr_in server_add;
    s->sock.sin_addr.s_addr = inet_addr(ipAddr);
    s->sock.sin_port = htons(port);
    s->sock.sin_family = AF_INET;
}

int bindSocket(Server *s)
{
    // initialize
    int reuseaddr = 1;
    if (setsockopt(s->socketfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)))
    {
        perror("setsockopt failed");
        return -1;
    }

    int result = bind(s->socketfd, (struct sockaddr *)&s->sock, sizeof(s->sock));
    if (result)
    {
        if (errno == EADDRINUSE)
        {
            printf("Address already in use!\n");
        }
        printf("%d Failed to bind to port!\n", errno);
        return -1;
    }
    printf("PID: %d \nPort %d bound on address %s !\n", getpid(), s->port, s->ipAddr);
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
            else
            {
                continue;
            }
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
        initHTTPRequest(&request);
        HTTPResponse response;
        initHTTPResponse(&response);
        int result = scanRequest(connfd, &request, &keepAlive, &statusCode);
        response.statusCode = statusCode;
        setRequest(&response, &request);
        if (result == -1)
        {
            printf("%d", statusCode);
        }
        const char *resp = "Hello there man!";
        setResponseBody(&response, (char *)resp, strlen(resp));
        sendResponse(&response, connfd);

        freeHTTPRequest(&request);
        freeHTTPResponse(&response);
    }
}
