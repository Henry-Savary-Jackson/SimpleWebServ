

#include "cc_pqueue.h"
#include "http.h"
#include "parser.h"


int chooseResponseContentEncoding(HTTPRequest *request, HTTPResponse *response)
{
    CC_Deque *acceptEncodings = getHeaderValues(request, ACCEPT_ENCODING_HEADER_NAME);
    if (acceptEncodings != NULL)
    {
        CC_PQueue *encodingPqueue = decodeAcceptEncodings(acceptEncodings);
        if (!encodingPqueue)
        {
            return -1;
        }
        int result = decideContentEncoding(encodingPqueue, &response->contentEncoding);
        if (response->contentEncoding == UNKNOWN_ENCODING)
        {
            response->statusCode = HTTP_NOT_ACCEPTED;
            response->contentType = "text/html";
            response->contentEncoding = IDENTITY_ENCODING;
            return -1;
        }
    }
    else
    {
        response->contentEncoding = IDENTITY_ENCODING;
    }
    return 0;
}

int chooseRequestTransferCoding(HTTPRequest *request, HTTPResponse *response)
{
    char *transferCodingStr = getHeader(request, TRANSFER_CODING_HEADER_NAME);
    if (!transferCodingStr)
    {
        return 0;
    }
    request->transferEncoding = HTTP_ENCODING(transferCodingStr);
    if (request->transferEncoding == UNKNOWN_ENCODING)
    {
        response->statusCode = HTTP_NOT_ACCEPTED;
        return -1;
    }
    return 0;
}

int chooseResponseTransferCoding(HTTPRequest *request, HTTPResponse *response )
{
    CC_Deque *acceptTE = getHeaderValues(request, TRANSFER_ENCODING_CLIENT_HEADER_NAME);
    if (acceptTE != NULL)
    {
        CC_PQueue *encodingPqueue = decodeAcceptEncodings(acceptTE);
        if (!encodingPqueue)
        {
            return -1;
        }
        if (decideTransferEncoding(encodingPqueue, &response->transferEncoding))
        {
            response->statusCode = HTTP_NOT_ACCEPTED;
            response->contentType = "text/html";
            response->contentEncoding = IDENTITY_ENCODING;
            response->transferEncoding = IDENTITY_ENCODING;
            return -1;
        }
    }
    return 0;
}

int prepareHTTPRequestMetadata(HTTPRequest *request, HTTPResponse* response)
{
    char *contentLengthS = getHeader(request, CONTENT_LENGTH_HEADER_NAME);
    char *contentEncodingS = getHeader(request, CONTENT_ENCODING_HEADER_NAME);

    if (!contentEncodingS)
    {
        request->contentEncoding = IDENTITY_ENCODING;
    }
    else
    {
        request->contentEncoding = HTTP_ENCODING(contentEncodingS);
        if (request->contentEncoding == UNKNOWN_ENCODING)
        {
            response->statusCode = HTTP_UNSUPPORTED_MEDIA_TYPE;
            return -1;
        }
    }

    if (contentLengthS)
    {
        const int base = 10;
        request->contentLength = (int)strtol(contentLengthS, NULL, base);
    }else{
        request->contentLength = 0;
    }

    return 0;
}



int encodingFilter(HTTPRequest* request, HTTPResponse * response, int connfd, void* args){
    int ret = 0;

    if ((ret = prepareHTTPRequestMetadata(request,response))){
        return ret;
    }

    if ((ret = chooseResponseContentEncoding(request, response)))
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return ret;
    }
    if ((ret = chooseResponseTransferCoding( request, response)))
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return ret;
    }
    if ((ret = chooseRequestTransferCoding(request, response)))
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return ret;
    }
    if (request->transferEncoding != CHUNKED && request->contentLength > 0){
        scanBody(request);
        // decompress if need be
    }

    char * cookieStr= getHeader(request, COOKIE_CLIENT_HEADER_NAME);
    if (cookieStr && (ret = parseCookies(cookieStr, request))){
        response->statusCode = HTTP_BAD_REQUEST;
        return ret;
    }

    char * contentType = getHeader(request,CONTENT_TYPE_HEADER_NAME);
    if (contentType && (ret = decodeRequestContentMimeType(contentType, &request->contentType) )){
        response->statusCode = HTTP_BAD_REQUEST;
        return ret;
    }

    return ret;
}
