
#include "cc_common.h"
#include "cc_hashtable.h"
#include "http.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <linux/limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

char *cookie_samesite_arr[3] = {COOKIE_SAMESITE_STRICT_STR, COOKIE_SAMESITE_LAX, COOKIE_SAMESITE_NONE};

void parseCookieHandle(char *inStr, void *args)
{
    HTTPRequest *request = args;

    int strLen = (int)strlen(inStr);

    char key[strLen];
    char value[strLen];

    int count = sscanf(inStr, " %[^=]=%s ", key, value);
    setCookieRequest(request, key, value);
}


int parseCookies(char *cookiesStr, HTTPRequest *request)
{
    tokenize(cookiesStr, ';', parseCookieHandle, (void *)request);
    return 0;
}

int encodeCookie(Cookie *cookie, char **output)
{
    size_t maxValueSize = strlen(cookie->name) + strlen(cookie->value) + strlen(" = ; ");

    size_t maxSize = 1024 + PATH_MAX + maxValueSize;

    size_t offset = 0;

    *output = custom_alloc(maxSize);

    int n_actual_size = snprintf(*output, maxValueSize, "%s=%s; ", cookie->name, cookie->value);
    if (n_actual_size < 0)
    {
        // error
        return n_actual_size;
    }
    offset += n_actual_size;

    if (offset >= maxSize)
    {
        return -1;
    }

    if (cookie->httpOnly)
    {
        n_actual_size = snprintf(*output + offset, maxSize - offset, "HttpOnly; ");
        if (n_actual_size < 0)
        {
            return n_actual_size;
        }
        offset += n_actual_size;
    }

    if (offset >= maxSize)
    {
        return -1;
    }

    n_actual_size = snprintf(*output + offset, maxSize - offset, "Path=%s; ", cookie->path);
    if (n_actual_size < 0)
    {
        return n_actual_size;
    }
    offset += n_actual_size;

    if (offset >= maxSize)
    {
        return -1;
    }

    if (cookie->secure)
    {

        n_actual_size = snprintf(*output + offset, maxSize - offset, "Secure; ");
        if (n_actual_size < 0)
        {
            return n_actual_size;
        }
        offset += n_actual_size;
    }


    if (offset >= maxSize)
    {
        return -1;
    }

    n_actual_size = snprintf(*output + offset, maxSize - offset, "Max-Age=%d; ", cookie->maxAge);
    if (n_actual_size < 0)
    {
        return n_actual_size;
    }
    offset += n_actual_size;

    if (offset >= maxSize)
    {
        return -1;
    }
    n_actual_size = snprintf(*output + offset, maxSize - offset, "SameSite=%s; ", COOKIE_SAMESITE_STR(cookie->samesite));
    if (n_actual_size < 0)
    {
        return n_actual_size;
    }

    return 0;
}

int encodeCookies(HTTPResponse *response)
{
    CC_HashTableIter iter;
    cc_hashtable_iter_init(&iter, response->cookies);

    enum cc_stat stat;
    TableEntry *entry = NULL;
    while ((stat = cc_hashtable_iter_next(&iter, &entry)) != CC_ITER_END)
    {
        if (!entry)
        {
            return -1;
        }
        if (stat != CC_OK)
        {
            return -1;
        }
        char *cookieValue;
        Cookie *cookie = entry->value;
        encodeCookie(cookie, &cookieValue);
        setHeader(response, COOKIE_SERVER_HEADER_NAME, cookieValue);
    }
    return 0;
}

Cookie *getCookieResponse(HTTPResponse *response, char *name)
{
    Cookie *out = NULL;
    enum cc_stat stat = cc_hashtable_get(response->cookies, name, (void **)&out);
    if (stat != CC_OK)
    {
        return NULL;
    }
    return out;
}

void setCookieResponse(HTTPResponse *response, Cookie *cookie)
{
    Cookie *cookieCopy = custom_alloc(sizeof(Cookie));
    memcpy(cookieCopy, cookie, sizeof(Cookie));
    enum cc_stat stat = cc_hashtable_add(response->cookies, custom_strdup(cookie->name), cookieCopy);
}

void initCookie(Cookie *cookie, char *name, char *value)
{
    cookie->httpOnly = true;
    cookie->name = name;
    cookie->path = "/";
    cookie->value = value;
    cookie->maxAge = COOKIE_DEFAULT_MAX_AGE;
    cookie->samesite = STRICT;
    cookie->secure = false;
}
