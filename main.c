#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()

#define MAX 1000
#define PORT 8080

void handleConn(int connfd) {
  char buf[MAX];
  int n;
  const char* resp = "Hello from server!\n";
  while (1){

    bzero(buf, MAX);
    recv(connfd, buf, MAX, 0);
    n = strnlen(buf, MAX);
    
    if (!strncmp(buf, "exit", n-1)){
        break;
    }
    printf("%s", buf);
    send(connfd,  resp, strnlen(resp, MAX), MSG_NOSIGNAL);
  }
  printf("Closing connection!");
  close(connfd);
}

int main() {
  uint sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (!sockfd) {
    printf("Failed to create socket.\n");
    exit(1);
  }
  printf("Socket created!\n");

  struct sockaddr_in server_add, cli;
  server_add.sin_addr.s_addr = htonl(INADDR_ANY);
  server_add.sin_port = PORT;
  server_add.sin_family = AF_INET;

  int result = bind(sockfd, (struct sockaddr *)&server_add, sizeof(server_add));
  if (result != 0) {
    printf("Failed to bind to port!");
    exit(1);
  }
  printf("Port bound!\n");

  printf("Listening...\n");
  while (1) {
    int result = listen(sockfd, 5);
    if (result != 0) {
      printf("Error listening!\n");
      exit(1);
    }

    int len = sizeof(cli);

    int connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);

    if (connfd < 0) {
      printf("Server accept failed!\n");
      exit(1);
    }
    printf("Accepted!\n");
    handleConn(connfd);
    printf("close\n");
  }

  printf("Closing!\n");
  close(sockfd);
  return 0;
}