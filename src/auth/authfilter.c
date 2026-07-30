#include "http.h"
#include "parser.h"
#include <server.h>
#include <auth.h>
#include <uuid/uuid.h>


int authFilter(HTTPRequest* request, HTTPResponse * response, int connfd){
    char * authCookie = getCookieRequest(request,AUTH_COOKIE_REMEMBER_ME_NAME  );
    if (authCookie == NULL){
        // handle no cookie found response
        makeUnauthorized(response);
        return -1;
    }

    int result = checkUserToken(&auth, authCookie);
    if (result){
        makeUnauthorized(response);
        return -2;
    }

    return 0;
}

int sessionIDFilter(HTTPRequest* request, HTTPResponse * response, int connfd){
    char * sessionID = getCookieRequest(request,AUTH_COOKIE_SESSIONID_NAME);

    if (!sessionID){
        uuid_t binSessionID;
        uuid_generate_random(binSessionID);
        char * newSessionID = custom_alloc(AUTH_SESSION_TOKEN_SIZE);
        uuid_unparse_lower(binSessionID, newSessionID);

        Cookie cookie;
        initCookie(&cookie, AUTH_COOKIE_SESSIONID_NAME, newSessionID );
        setCookieResponse(response, &cookie);
    }
    return 0;

}
