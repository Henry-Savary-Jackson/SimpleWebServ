#include "http.h"
#include "server.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
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
#include <unistd.h>

#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#define MAX_WORKERS 10
#define PORT 8080

Server server;

void sighandler_parent(int signal) {
  killChildrenProcesses(&server);
  freeServer(&server);
}

void chlddeth(int signal) { printf("Death of child!\n"); }

int main(int argc, char *argv[]) {
  init_code_to_phrase();

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

  struct sigaction sa2;
  sa2.sa_handler = chlddeth;
  sa2.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa2, NULL);

  initServer(&server, "localhost", "mywebserver", "webroot");
  setServerAddress(&server, addr, port);
  bindSocket(&server);
  launch(&server);
  freeServer(&server);
  hmfree(code_to_phrase);
  // kill all worker processes

  return 0;
}
