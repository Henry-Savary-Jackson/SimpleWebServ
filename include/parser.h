#pragma once


#include "http.h"
int scanRequest(int connfd, HTTPRequest *request, bool *keepAlive,
                 int *status) ;

int sendResponse(HTTPResponse *response, int connfd) ;
int encodeHeaders(Header *headers, char **buffer) ;