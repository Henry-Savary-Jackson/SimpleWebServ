#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <linux/limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> // read(), write(), close()

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define MAX 4096
#define MAX_WORKERS 10
#define MAX_CONN 5
#define PORT 8080
#define SIMPLE_MAX 32

uint sockfd = 0;

int *childrenArr = NULL;

void sighandler_parent(int signal) {
  close(sockfd);
  int len = arrlen(childrenArr);
  int s;
  for (int i = 0; i < len; i++) {
    int pid_child = childrenArr[i];
    printf("killing child %d", pid_child );
    kill(pid_child, SIGTERM);
    waitpid(pid_child, &s, 0);
  }
}

void handleConn(int connfd) {
  char buf[MAX];
  int readoffset = 0;
  int writeoffset = 0;
  int n;
  char currentLine[MAX];
  bzero(currentLine, MAX);
  int nCurrentLine = 0;

  struct {
    char *key;
    char *value;
  } *headers;
  sh_new_arena(headers);

  char lineDone = 0;

  int n_left = MAX;
  const char *resp = "HTTP/1.1 200 OK\r\n\r\nHello there!";
  while (1) {
    // if the buffer is full make space
    if (writeoffset >= MAX) {
      int n_chars_left = writeoffset - readoffset;
      memmove(buf, buf + readoffset, n_chars_left);
      bzero(buf + n_chars_left, MAX - n_chars_left);
      writeoffset = n_chars_left;
      readoffset = 0;
    }

    writeoffset += recv(connfd, buf, MAX - writeoffset, 0);
    while (readoffset <= writeoffset) {
      char c = buf[readoffset];
      currentLine[readoffset] = c;
      readoffset++;
      if (c == '\n') {
        lineDone = 1;
      }
    };

    if (lineDone == 1) {
      // handle first line
      lineDone = 0;
      char uri[PATH_MAX];
      char method[SIMPLE_MAX];
      int verMajor = 0;
      int verMinor = 0;
      uint result = sscanf(currentLine, "%31s %4095s HTTP/%d.%d", method, uri,
                           &verMajor, &verMinor);
      if (result != 4) {
        printf("error!");
        goto end;
      }
      printf("version %d.%d\n", verMajor, verMinor);
      printf("%s %s\n", uri, method);
      send(connfd, resp, strlen(resp), MSG_NOSIGNAL);
      goto end;
    }

  }
  goto end;
end:
  printf("Closing connection!");
  shfree(headers);
  close(connfd);
  exit(0);
}

int main(int argc, char *argv[]) {

  if (argc <= 2) {
    printf("Too few arguments!\n");
    exit(-1);
  }
  char addr[100];
  uint port = 8080;

  const char *opts = "b:p:";
  char opt;

  while ((opt = getopt(argc, argv, opts)) != -1) {
    switch (opt) {
    case 'b':
      strncpy(addr, optarg, sizeof(addr));
      break;
    case 'p':
      port = atoi(optarg);
      break;
    }
  }

  // make sure all children share the same pgid
  struct sigaction sa;
  sa.sa_handler = sighandler_parent;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  uint pid = getpid();

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (!sockfd) {
    printf("Failed to create socket.\n");
    exit(1);
  }
  printf("Socket created!\n");

  struct sockaddr_in server_add, cli;
  server_add.sin_addr.s_addr = inet_addr(addr);
  server_add.sin_port = htons(port);
  server_add.sin_family = AF_INET;

  int result = bind(sockfd, (struct sockaddr *)&server_add, sizeof(server_add));
  if (result != 0) {
    if (errno == EADDRINUSE) {
      printf("Address already in use!\n");
    }
    printf("%d Failed to bind to port!\n", errno);
    goto end;
  }
  printf("PID: %d \nPort %d bound on address %s !\n", pid, port, addr);

  printf("Listening...\n");
  while (1) {
    int result = listen(sockfd, MAX_CONN);
    if (result) {
      printf("Error listening!\n");
      goto end;
    }

    int len = sizeof(cli);

    int connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);

    if (connfd < 0) {
      printf("Server accept failed!\n");
      goto end;
    }
    printf("Accepted!\n");
    // create a child processs to handle new connection
    int worker_pid = fork();
    if (worker_pid == 0) {
      // will handle termination normally
      sa.sa_handler = SIG_DFL;
      sigaction(SIGTERM, &sa, NULL);
      sigaction(SIGINT, &sa, NULL);
      handleConn(connfd);
    } else {
      arrput(childrenArr, worker_pid);
    }
  }
  goto end;
end:
  printf("Stopping server!\n");

  close(sockfd);
  // kill all worker processes
  kill(0, SIGTERM);

  return 0;

  return 0;
}
