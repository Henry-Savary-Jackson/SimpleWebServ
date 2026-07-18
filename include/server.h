#pragma once

#include "threadpool.h"
#include <netinet/in.h>
typedef struct {
  char *webroot;
  char *name;
  char *host;
  int port;
  int maxWorkers;
  char* ipAddr;
  struct sockaddr_in sock;
  int socketfd;
  ThreadPool pool;
} Server;

void initServer(Server* server, char* host, char* name, char* webroot , int maxWorkers);
void setServerAddress(Server *server, char *ipAddr, int port) ;
int bindSocket(Server* server);
void freeServer(Server* server);
void handleConnection(int connfd, Server *server) ;

void launch(Server *server) ;
