#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
// #include "stb_ds.h"

#define STB_DS

#define MAX 4096
#define PORT 8000

int main() {

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (!sock) {
    printf("Failed to create socket!");
    exit(1);
  }
  printf("Socket created!\n");
  struct sockaddr_in server_addr;

  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = PORT;
  server_addr.sin_family = AF_INET;

  int result =
      connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (result) {
    printf("Failed to connect!\n");
    exit(1);
  }
  printf("Connected!\n");

  char buf[MAX];
  char inBuf[MAX];
  int n;

  while (1) {
    bzero(buf, MAX);
    bzero(inBuf, MAX);

    fgets(buf, MAX, stdin);
    printf("Writing message!\n");
    n = strnlen(buf, MAX);
    int result = send(sock, buf, n, MSG_NOSIGNAL);
    if (result < 0) {
      printf("Failed to write to socket!\n");
      break;
    }
    if (!strncmp(buf, "exit", n-1)){
        break;
    }

    result = recv(sock, inBuf, MAX, 0);
    if (result < 0) {
      printf("Failure at reading from server!\n");
      break;
    }
    n = strnlen(inBuf, MAX);
    printf("%s", inBuf);
  }
  close(sock);
  printf("closing!\n");
}
