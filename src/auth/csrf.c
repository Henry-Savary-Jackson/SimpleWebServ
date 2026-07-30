

#include "cc_common.h"
#include "cc_hashtable.h"
#include "http.h"
#include "auth.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <sys/types.h>
#include <uuid/uuid.h>

int csrfHandler(HTTPRequest *request, HTTPResponse *response, void *handler, int connfd){
    AuthHandler* authHandler = handler;

    char * sessionID = getCookieRequest(request, AUTH_COOKIE_SESSIONID_NAME);
    if (!sessionID){
        sessionID = custom_alloc(1<<7);
        uuid_generate_random((u_char*)sessionID);
        goto make_new_csrf;
    }

    char * csrfToken = getCSRFForSession(sessionID);
    if (!csrfToken){
        goto make_new_csrf;
    }


send_resp:
    setResponseBody(response, csrfToken, AUTH_CSRF_TOKEN_SIZE-1);
    sendResponse(response,  connfd);

    return 0;

make_new_csrf:
    // get session token;
    csrfToken = addCSRFToSession(sessionID);
    goto send_resp;
}

Route getCSRFRoute(){
    Route route;
    Path path;
    stringToPath(&path, "/csrf");
    enum http_method method = GET;
    initRoute(&route, path, &method, 1);
    route.handleCallback = csrfHandler;
    route.handlerObject = NULL;
    return route;
}

int csrfFilter(HTTPRequest* request, HTTPResponse * response, int connfd){
    // try query paramso
    char * givenCSRF = getQueryParam(request, AUTH_CSRF_FORM_TOKEN_NAME);
    if (givenCSRF){
        // now check it
        goto check_csrf;
    }
    givenCSRF = getHeader(request, AUTH_CSRF_HEADER_TOKEN_NAME);

    if (!givenCSRF){
        goto error;
    }

check_csrf:
    char *sessionID = getCookieRequest(request, AUTH_COOKIE_SESSIONID_NAME);
    if (!sessionID){
        goto error;
    }
    char* trueCSRF = getCSRFForSession(sessionID);
    if (!trueCSRF || strcmp(trueCSRF, givenCSRF) != 0 ){
        goto error;
    }
    return 0;

error:
    response->statusCode = HTTP_FORBIDDEN;
    return -1;
}
char *getCSRFForSession(char *sessionId){

    char * csrf;
    enum cc_stat stat = cc_hashtable_get(sessionIDToCSRF, sessionId, (void**)&csrf);
    if (stat != CC_OK){
        return NULL;
    }
    return csrf;
}
char *addCSRFToSession(char *sessionId){
    char * csrfPtr = custom_alloc(AUTH_CSRF_TOKEN_SIZE);
    uuid_t binCSRF;
    uuid_generate_random(binCSRF);
    uuid_unparse_lower(binCSRF, csrfPtr);
    enum cc_stat stat_add_new = cc_hashtable_add(sessionIDToCSRF,sessionId, csrfPtr);
    if (stat_add_new != CC_OK){
        // some error
        return NULL;

    }
    return csrfPtr;
}
