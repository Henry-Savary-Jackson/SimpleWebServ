#include "server.h"
#include "http.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <parser.h>
#include <signal.h>
#include <stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CONN 5

void initServer(Server *s, char *host, char *name, char *webroot) {
  s->host = strdup(host);
  s->name = strdup(name);
  s->webroot = strdup(webroot);
  s->port = -1;
  s->ipAddr = NULL;
  s->socketfd = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&s->sock, sizeof(struct sockaddr_in));
}

void freeServer(Server *s) {
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
  if (s->childrenProcesses)
    killChildrenProcesses(s);

  arrfree(s->childrenProcesses);
}

void setServerAddress(Server *s, char *ipAddr, int port) {
  s->port = port;
  if (s->ipAddr)
    free(s->ipAddr);
  s->ipAddr = strdup(ipAddr);
  struct sockaddr_in server_add;
  s->sock.sin_addr.s_addr = inet_addr(ipAddr);
  s->sock.sin_port = htons(port);
  s->sock.sin_family = AF_INET;
}

int bindSocket(Server *s) {
  // initialize
  int reuseaddr = 1;
  if (setsockopt(s->socketfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                 sizeof(reuseaddr))) {
    perror("setsockopt failed");
    return -1;
  }

  int result = bind(s->socketfd, (struct sockaddr *)&s->sock, sizeof(s->sock));
  if (result) {
    if (errno == EADDRINUSE) {
      printf("Address already in use!\n");
    }
    printf("%d Failed to bind to port!\n", errno);
    return -1;
  }
  printf("PID: %d \nPort %d bound on address %s !\n", getpid(), s->port, s->ipAddr);
  return 0;
}

void launch(Server *server) {

  struct sigaction sa;
  sa.sa_handler = SIG_DFL;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  int listen_result = listen(server->socketfd, MAX_CONN);
  if (listen_result) {
    printf("Error listening!\n");
    goto end;
  }
  while (1) {
    struct sockaddr_in cli;
    int len = sizeof(cli);

    int connfd =
        accept(server->socketfd, (struct sockaddr *)&cli, (socklen_t *)&len);

    if (connfd < 0) {
      if (errno != EINTR) {
        printf("\npid:%d Server accept failed!\n", getpid());
        goto end;
      } else {
        continue;
      }
      int worker_pid = fork();
      if (worker_pid == 0) {
        // will handle termination normally
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        handleConnection(connfd, server);

      } else if (worker_pid > 0) {
        arrput(server->childrenProcesses, worker_pid);
      } else {
        // error
        printf("Fork failed!\n");
        goto end;
      }
    }
  }

end:
  printf("Stopping server \'%s\'!\n", server->name);
  return;
}

void killChildrenProcesses(Server *server) {
  int len = arrlen(server->childrenProcesses);
  int s;
  for (int i = 0; i < len; i++) {
    int pid_child = server->childrenProcesses[i];
    printf("killing child %d %d\n", pid_child, getpid());
    kill(pid_child, SIGTERM);
    waitpid(pid_child, &s, 0);
  }
}

void handleConnection(int connfd, Server *server) {
  bool keepAlive = true;
  int statusCode = 200;
  while (keepAlive) {

    HTTPRequest request;
    initHTTPRequest(&request);
    HTTPResponse response;
    initHTTPResponse(&response);
    response.request = &request;
    int result = scanRequest(connfd, &request, &keepAlive, &statusCode);
    response.statusCode = statusCode;
    if (result == -1){
        printf("%d", statusCode);
        // error;
    }


    freeHTTPRequest(&request);
    freeHTTPResponse(&response);
  }
}