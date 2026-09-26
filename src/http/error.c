#include "cc_hashtable.h"
#include "parser.h"
#include "server.h"
#include <http.h>
#include <stdio.h>
#include <string.h>

int makeErrorResponse(HTTPResponse *response, int status, char *message)
{
    response->contentType = "text/plain";
    response->contentEncoding = IDENTITY_ENCODING;
    response->transferEncoding = IDENTITY_ENCODING;
    response->statusCode = status;
    if (message != NULL)
    {
        setResponseBody(response, message, strlen(message));
    }
    cc_hashtable_remove_all(response->cookies);
    // prepareHTTPResponseMetadata(response);
    return 0;
}
int makeBadRequest(HTTPResponse *response, char * reason)
{
    return makeErrorResponse(response, HTTP_BAD_REQUEST, reason);
}
int makeUnauthorized(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_UNAUTHORIZED, reason);
}
int makeForbidden(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_FORBIDDEN, reason);
}
int makeNotFound(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_NOT_FOUND, reason);
}
int makeContentLengthRequired(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_CONTENT_LENGTH_REQUIRED, reason);
}
int makeMethodNotSupported(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_METHOD_UNSUPPORTED, reason);
}
int makeNotAccepable(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_NOT_ACCEPTED, reason);
}
int makeMediaTypeNotSupported(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_UNSUPPORTED_MEDIA_TYPE,reason);
}
int makeServerError(HTTPResponse *response, char * reason)
{
    return makeErrorResponse(response, HTTP_SERVER_ERROR, reason);
}


int makeRangeUnSatisfiable(HTTPResponse *response, char *reason, size_t trueSize){
    char rangeHeader[1<<7];
    snprintf(rangeHeader, sizeof(rangeHeader), "bytes */%ld", trueSize);
    setHeader(response, CONTENT_RANGE_HEADER_NAME, rangeHeader);
    return makeErrorResponse(response, HTTP_RANGE_NOT_SATISFIABLE, reason);
}
