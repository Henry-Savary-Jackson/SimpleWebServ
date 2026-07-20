#pragma once

#include "cc_array.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "threadpool.h"
#include <netinet/in.h>

typedef struct
{
    CC_Array *requestHandlers;
    CC_DynamicPool *pool;
}Router;

typedef struct
{
    char *webroot;
    char *name;
    char *host;
    int port;
    int maxWorkers;
    char *ipAddr;
    struct sockaddr_in sock;
    int socketfd;
    ThreadPool pool;
    CC_DynamicPool *arena;
    Router router;
}Server;



typedef struct{
  Path webroot;
  Path pathPrefix;
  enum http_encoding* acceptedEncodings;
} FileSystemHandler ;

typedef struct {
  int (*handleCallback)( HTTPRequest* request, HTTPResponse* response ,void* handlerObj);
  void* handlerObject;
  int (*getPrefixMatch)(HTTPRequest*, void* handlerObj);
}RequestHandler;


void initServer(Server** server_p, char* host, char* name, char* webroot , int maxWorkers);
void setServerAddress(Server *server, char *ipAddr, int port) ;
int bindSocket(Server* server);
void freeServer(Server* server);
void handleConnection(int connfd, Server *server) ;
void launch(Server *server) ;
void shutDownServer(Server* server);


void initRouter(Router *router, CC_DynamicPool *pool);
void addHandler(Router* router, RequestHandler* handler );
RequestHandler* longestPrefixMatch(Router* router, HTTPRequest* request);
void addFileSystemHandlerServer(Server* server, FileSystemHandler* handler);
void freeRouter(Router* router);

void initFileSystemHandler(FileSystemHandler* fsHandler, char * pathPrefix, char* webroot, CC_DynamicPool* pool);
void addFileSystemHandler(Router* router, FileSystemHandler* fsHandler);
