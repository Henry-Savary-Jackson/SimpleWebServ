#include "http.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <asm-generic/errno-base.h>
#include <auth.h>
#include <errno.h>
#include <stdio.h>
#include <uuid/uuid.h>

int readFile(char *path, HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{

    FILE *openfile;
    do
    {
        openfile = fopen(path, "r");
        if (!openfile)
        {
            switch (errno)
            {
            case EINTR:
                break;
            case EACCES:
                makeNotFound(response);
                return -1;
            default:
                makeServerErrror(response);
                return -1;
            }
        }
    } while (!openfile);

    response->contentType = getMimeTypeForFile(path);

    const int FS_CHUNK_SIZE = 1 << 12;

    GrowingBuffer bufferBody;
    initGrowingBuffer(&bufferBody, HTTP_STREAM_INIT_BUFFER);


    char chunk[FS_CHUNK_SIZE];
    int n_chars = 0;
    while ((n_chars = readFromFile(openfile, chunk, FS_CHUNK_SIZE)) > 0)
    {
        appendGrowingBuffer(&bufferBody, chunk, n_chars);
    }
    setResponseBody(response, bufferBody.ptr, bufferBody.size);


    sendResponse(response, connfd);
    return 0;
}


int adminFormCallback(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    AuthHandler *authHandler = handler;
    return readFile(authHandler->adminHTMLLocation, request, response, handler, connfd);
}


int loginHandlerCallback(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    AuthHandler *authHandler = handler;
    char *username = getQueryParam(request, "username");

    if (!username)
    {
        makeBadRequest(response);
        return -1;
    }

    char *password = getQueryParam(request, "password");
    if (!password)
    {
        makeBadRequest(response);
        return -1;
    }

    char * token;
    char * role;
    int ret = loginUser(&auth, username, password, &token, &role);
    switch (ret){
        case 0:
            break;
        case -2:
            makeNotFound(response);
            return -1;
        default:
            makeServerErrror(response);
            return -1;
    }

    Cookie rememberMe;
    initCookie(&rememberMe, AUTH_COOKIE_REMEMBER_ME_NAME, token);
    setCookieResponse(response, &rememberMe );

    Cookie roleCookie;
    initCookie(&roleCookie, AUTH_COOKIE_ROLE_NAME , role);
    roleCookie.httpOnly = false;
    setCookieResponse(response, &roleCookie );

    Cookie usernameCookie;
    initCookie(&usernameCookie, AUTH_COOKIE_USERNAME_NAME , username);
    usernameCookie.httpOnly = false;
    setCookieResponse(response, &usernameCookie );

    return 0;
}
int SignUpHandlerCallback(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd)
{
    AuthHandler *authHandler = handler;
    char *username = getQueryParam(request, "username");

    if (!username)
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return -1;
    }
    char *password = getQueryParam(request, "password");
    if (!password)
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return -1;
    }
    char *role = getQueryParam(request, "role");
    if (!role)
    {
        response->statusCode = HTTP_BAD_REQUEST;
        return -1;
    }

    char *token;
    int ret = signUpUser(&auth, username, password, role,&token);
    if (ret)
    {
        response->statusCode = HTTP_SERVER_ERROR;
        return -1;
    }

    // setCookie(request, AUTH_COOKIE_REMEMBER_ME_NAME, token);

    return 0;
}
Route getLoginRoute(AuthHandler *authHandler)
{

    Route route;
    enum http_method method = POST;
    Path pathLogin;
    stringToPath(&pathLogin, authHandler->loginURI);
    initRoute(&route, pathLogin, &method, 1);
    route.handleCallback = loginHandlerCallback;
    route.handlerObject = authHandler;
    addFilterToChain(&route, csrfFilter, NULL);
    return route;
}

Route getSignUpRoute(AuthHandler *authHandler)
{

    Route route;
    enum http_method method = POST;
    Path pathLogin;
    stringToPath(&pathLogin, authHandler->signUpURI);
    initRoute(&route, pathLogin, &method, 1);
    route.handleCallback = SignUpHandlerCallback;
    route.handlerObject = authHandler;
    addFilterToChain(&route, csrfFilter, NULL);
    addFilterToChain(&route, authFilter, NULL);
    addRoleFilter(&route, (char **)"admin", 1);

    // only admins can add other users
    return route;
}

Route getAdminPageRoute(AuthHandler *authHandler)
{
    Route route;
    enum http_method method = GET;
    Path pathLogin;
    stringToPath(&pathLogin, authHandler->loginURI);
    initRoute(&route, pathLogin, &method, 1);
    route.handleCallback = adminFormCallback;
    route.handlerObject = authHandler;
    return route;
}


void addAuthenticationHandler(Server* server, AuthHandler *authHandler)
{

    Route route = getLoginRoute(authHandler);
    addRoute(&server->router, &route);
    route = getSignUpRoute(authHandler);
    addRoute(&server->router, &route);
    route = getAdminPageRoute(authHandler);
    addRoute(&server->router, &route);
}

void initAuthHandler(AuthHandler *handler,
                     char *loginURI,
                     char *signUpURI,
                     char *adminURLPrefix,
                     char *adminHTMLLocation
                     )
{
    handler->adminHTMLLocation = adminHTMLLocation;
    handler->adminURLPrefix = adminURLPrefix;
    handler->loginURI = loginURI;
    handler->signUpURI = signUpURI;
}
