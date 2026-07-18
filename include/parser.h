#pragma once


#include "cc_hashtable.h"
#include "http.h"
#include "utils.h"
int scanRequest(int connfd, HTTPRequest *request, bool *keepAliv, int* status) ;

int sendResponse(HTTPResponse *response, int connfd) ;
int encodeHeaders(CC_HashTable* headers, GrowingBuffer* buffer) ;
