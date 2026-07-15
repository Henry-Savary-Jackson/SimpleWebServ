#pragma once

#include <netinet/in.h>
typedef struct {
  char *webroot;
  char *name;
  char *host;
  int port;
  char* ipAddr;
  struct sockaddr_in sock;
  int socketfd;
  int* childrenProcesses;
} Server;

void initServer(Server* s, char* host, char* name, char* webroot );
void setServerAddress(Server *s, char *ipAddr, int port) ;
int bindSocket(Server* s);
void freeServer(Server* s);
void handleConnection(int connfd, Server *server) ;
void killChildrenProcesses(Server *server) ;

void launch(Server *server) ;