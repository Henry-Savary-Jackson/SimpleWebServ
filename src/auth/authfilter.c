#include "cc_array.h"
#include "http.h"
#include "parser.h"
#include <auth.h>
#include <server.h>
#include <uuid/uuid.h>


int authFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void *args)
{
    char *authCookie = getCookieRequest(request, AUTH_COOKIE_REMEMBER_ME_NAME);
    if (authCookie == NULL)
    {
        // handle no cookie found response
        makeUnauthorized(response, "Remember-me cookie is missing!");
        return -1;
    }

    int result = checkUserToken(&auth, authCookie);
    if (result)
    {
        makeUnauthorized(response, "Remember-me token is incorrect!");
        return -2;
    }

    return 0;
}

int roleFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void *args)
{
    struct
    {
        char **roles;
        int n_roles;
    } *allowed_roles = args;

    for (int i = 0; i < allowed_roles->n_roles; i++)
    {
        if (!strcmp(current_user.role, allowed_roles->roles[i]))
        {
            return 0;
        }
    }

    makeForbidden(response, "User doesn't have the required role!");
    return -1;
}

void addRoleFilter(Route* route,  char** roles, int n){
    struct {char** roles; int n;}* obj = glbl_custom_alloc(sizeof(char**)+ sizeof(int));
    obj->roles = (char**)glbl_custom_calloc(n, sizeof(char*));
    memcpy((void*)obj->roles, (void*)roles, sizeof(char*)*n);
    obj->n = n;
    addFilterToChain(route, roleFilter, obj);
}

int sessionIDFilter(HTTPRequest *request, HTTPResponse *response, int connfd, void *args)
{
    char *sessionID = getCookieRequest(request, AUTH_COOKIE_SESSIONID_NAME);

    if (!sessionID)
    {
        sessionID = generateSessionID();
        Cookie cookie;
        initCookie(&cookie, AUTH_COOKIE_SESSIONID_NAME, sessionID);
        setCookieResponse(response, &cookie);
    }
    current_user.sessionID = sessionID;
    return 0;
}
