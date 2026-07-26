

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <sys/socket.h>
int sendDataTCP(int connfd, char * chunk , int size){
    int nTotalSent =0;
    while ( nTotalSent < size){
        int nSent = (int)send(connfd, chunk+nTotalSent,size-nTotalSent, MSG_NOSIGNAL);
        if (nSent < 0 && errno == EINTR){
            continue;
        }
        if (nSent < 0){
            return -1;
        }
        nTotalSent += nSent;
    }
    return 0;
}
