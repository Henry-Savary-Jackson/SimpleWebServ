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

Server *server;

void sighandler_parent(int signal)
{
    printf("Shutting down thread pool!");
    if (server)
    {
        shutDownServer(server);
    }
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


    initServer(&server, "localhost", "mywebserver", "webroot", MAX_WORKERS);
    setServerAddress(server, addr, (int)port);

    FileSystemHandler handler;
    initFileSystemHandler(&handler, "/test", "webroot",server->arena);
    addFileSystemHandlerServer(server, &handler);
    bindSocket(server);

    launch(server);

    cc_hashtable_destroy(code_to_phrase);

    // kill all worker processes

    return 0;
}
