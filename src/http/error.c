#include "cc_hashtable.h"
#include "parser.h"
#include <http.h>
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
int makeServerErrror(HTTPResponse *response, char * reason)
{

    return makeErrorResponse(response, HTTP_SERVER_ERROR, reason);
}
