#pragma once

#include "cc_array.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "threadpool.h"
#include <netinet/in.h>

extern thread_local Arena arena ;

void* custom_alloc(size_t size);
void* custom_calloc(size_t blocks,size_t size);
void custom_free(void* ptr);
char* custom_strdup(char* str);

typedef struct
{
    CC_Array *requestHandlers;
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
    Router router;
}Server;



typedef struct{
  Path webroot;
  Path pathPrefix;
  enum http_encoding* acceptedEncodings;
} FileSystemHandler ;

typedef struct {
  int (*handleCallback)( HTTPRequest* request, HTTPResponse* response ,void* handlerObj, int connfd);
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


void initRouter(Router *router);
void addHandler(Router* router, RequestHandler* handler );
RequestHandler* longestPrefixMatch(Router* router, HTTPRequest* request);
void addFileSystemHandlerServer(Server* server, FileSystemHandler* handler);
void freeRouter(Router* router);

void initFileSystemHandler(FileSystemHandler* fsHandler, char * pathPrefix, char* webroot);
void addFileSystemHandler(Router* router, FileSystemHandler* fsHandler);
