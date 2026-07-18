#include "cc_hashtable.h"
#include "http.h"
#include "server.h"
#include "threadpool.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <linux/limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_WORKERS 10
#define PORT 8080
#define IP_ADDR_SIZE 32

Server server;

void sighandler_parent(int signal)
{
    printf("Shutting down thread pool!");
    shutDownThreadPool(&server.pool);
    freeServer(&server);
}

void chlddeth(int signal)
{
    printf("Death of child!\n");
}

int main(int argc, char *argv[])
{
    init_code_to_phrase();

    char addr[IP_ADDR_SIZE];
    uint port = PORT;

    const char *opts = "b:p:";
    char opt;

    while ((opt = getopt(argc, argv, opts)) != -1)
    {
        switch (opt)
        {
        case 'b':
            if (!optarg)
            {
                strcpy(addr, "0.0.0.0");
            }
            else
            {
                strncpy(addr, optarg, sizeof(addr));
            }
            break;
        case 'p':
            if (optarg)
            {
                port = atoi(optarg);
            }
            break;

        default:
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

    // Header *headers;
    // > Host: localhost:8000
    // > User-Agent: curl/8.19.0
    // > Accept: */*
    // > Content-Length: 63
    // > Content-Type: application/x-www-form-urlencoded
    // >

    // const char *const_type = "Content-Type";
    // char c[100];
    // memcpy(c, const_type, strlen(const_type));
    // sh_new_arena(headers);
    // shput(headers, strdup("Host"), strdup("localhost:8000"));
    // shput(headers, strdup("User-Agent"), strdup("curl/8.19.0"));
    // shput(headers, strdup("Accept"), strdup("*/*"));
    // shput(headers, strdup("Content-Length"), strdup("63"));
    // shput(headers, strdup(const_type), strdup("application/x-www-form-urlencoded"));

    // printf("Result:%s", shget(headers,const_type));
    initServer(&server, "localhost", "mywebserver", "webroot", MAX_WORKERS);
    setServerAddress(&server, addr, (int)port);
    bindSocket(&server);
    launch(&server);
    cc_hashtable_destroy(code_to_phrase);
    // kill all worker processes

    return 0;
}
