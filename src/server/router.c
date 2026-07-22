
#include "cc_array.h"
#include "memory/cc_dynamic_pool.h"
#include "server.h"
#include "utils.h"

void initRouter(Router *router)
{
    CC_ArrayConf conf;
    cc_array_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;
    cc_array_new_conf(&conf, (&router->requestHandlers));
}

void addHandler(Router *router, RequestHandler *handler)
{
    RequestHandler *newPtr = custom_alloc(sizeof(RequestHandler));
    memcpy(newPtr, handler, sizeof(RequestHandler));
    cc_array_add(router->requestHandlers, newPtr);
}
RequestHandler *longestPrefixMatch(Router *router, HTTPRequest *request)
{
    int maxMatch = 0;
    RequestHandler *longestMatchHandler = NULL;

    CC_ArrayIter iterator;
    cc_array_iter_init(&iterator, router->requestHandlers);
    RequestHandler *currentHandler;
    enum cc_stat stat;
    while ((stat = cc_array_iter_next(&iterator, (void **)&currentHandler)) != CC_ITER_END)
    {
        int match = currentHandler->getPrefixMatch(request, currentHandler->handlerObject);
        if (match > maxMatch)
        {
            maxMatch = match;
            longestMatchHandler = currentHandler;
        }
    }
    return longestMatchHandler;
}
void freeRouter(Router *router)
{
    // cc_array_destroy(router->requestHandlers);
}


int handleNoHandlerFound();
