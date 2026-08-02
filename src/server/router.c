
#include "cc_array.h"
#include "cc_common.h"
#include "auth.h"
#include "http.h"
#include "memory/cc_dynamic_pool.h"
#include "parser.h"
#include "server.h"
#include "utils.h"

void initRouter(Router *router)
{
    CC_ArrayConf conf;
    cc_array_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;
    cc_array_new_conf(&conf, (&router->routes));
}

void addRoute(Router *router, Route *handler)
{
    Route *newPtr = custom_alloc(sizeof(Route));
    memcpy(newPtr, handler, sizeof(Route));
    cc_array_add(router->routes, newPtr);
}

int longestPrefixMatch(Router *router, HTTPRequest *request, Route** chosenRoute)
{
    int maxMatch = -1;
    bool oneMatch = false;

    CC_ArrayIter iterator;

    Route * currentRoute =NULL;
    *chosenRoute = NULL;
    bool oneFound = false;

    cc_array_iter_init(&iterator, router->routes);
    enum cc_stat stat;

    while ((stat = cc_array_iter_next(&iterator, (void **)&currentRoute)) != CC_ITER_END)
    {
        int match = prefixMatchPaths(&currentRoute->prefixPattern, &request->uriPath);

        if (match <= maxMatch){
            continue;
        }
        if (checkHTTPMethod(currentRoute, request)){
            oneFound = true;
            continue;
        }
        maxMatch = match;
        *chosenRoute = currentRoute;
    }
    if (*chosenRoute){
        return 0;
    }

    return  oneFound ? -2 : -1;

}
void freeRouter(Router *router)
{
    // cc_array_destroy(router->requestHandlers);
}

int handleRequestRouter(Route *route, HTTPRequest *request, HTTPResponse *response, int connfd)
{
    CC_ArrayIter iter;
    cc_array_iter_init(&iter, route->filterChain);

    struct {
    int (*handle)(HTTPRequest *request, HTTPResponse *response, int connfd, void * args);
    void * args;
    }* filter;
    enum cc_stat stat;
    while ((stat = cc_array_iter_next(&iter, (void **)&filter)) != CC_ITER_END)
    {
        // if
        if (stat != CC_OK)
        {
            return -1;
        }
        int result = filter->handle(request, response, connfd, filter->args);
        //
        if (result < 0)
        {
            return -2;
        }
    }
    return route->handleCallback(request, response, route->handlerObject, connfd);
}

void addFilterToChain(Route* route, int (*handle)(HTTPRequest *, HTTPResponse *, int , void * ), void* args){
    struct {
        int (*handle)(HTTPRequest *, HTTPResponse *, int , void * );
        void* args;
    } * filterObj = custom_alloc(sizeof(handle)+ sizeof(args));
    filterObj->args = args;
    filterObj->handle = handle;
    cc_array_add(route->filterChain, filterObj);
}

void initRoute(Route *route, Path prefixPattern, enum http_method *supportedMethods, int numSupportedMethods)
{
    route->prefixPattern = prefixPattern;
    route->handleCallback = NULL;
    route->handlerObject = NULL;
    CC_ArrayConf conf;
    cc_array_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;
    cc_array_new_conf(&conf, &route->filterChain);
    cc_array_new_conf(&conf, &route->allowedMethods);

    addFilterToChain(route, encodingFilter, NULL);
    addFilterToChain(route, sessionIDFilter, NULL);
    for (int i = 0; i < numSupportedMethods; i++)
    {
        enum http_method *method_p = custom_alloc(sizeof(enum http_method));
        *method_p = supportedMethods[i];
        cc_array_add(route->allowedMethods, method_p);
    }
}
