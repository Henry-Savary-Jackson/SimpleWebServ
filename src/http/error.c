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
int makeBadRequest(HTTPResponse *response)
{
    return makeErrorResponse(response, HTTP_BAD_REQUEST, NULL);
}
int makeUnauthorized(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_UNAUTHORIZED, NULL);
}
int makeForbidden(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_FORBIDDEN, NULL);
}
int makeNotFound(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_NOT_FOUND, NULL);
}
int makeContentLengthRequired(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_CONTENT_LENGTH_REQUIRED, NULL);
}
int makeMethodNotSupported(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_METHOD_UNSUPPORTED, NULL);
}
int makeNotAccepable(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_NOT_ACCEPTED, NULL);
}
int makeMediaTypeNotSupported(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_UNSUPPORTED_MEDIA_TYPE, NULL);
}
int makeServerErrror(HTTPResponse *response)
{

    return makeErrorResponse(response, HTTP_SERVER_ERROR, NULL);
}
