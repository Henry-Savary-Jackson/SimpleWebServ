
#include "cc_common.h"
#include "cc_deque.h"
#include "memory/cc_dynamic_pool.h"
#include "server.h"
#include "utils.h"
#include <linux/limits.h>
#include <string.h>

void initPath(Path *path)
{
    path->isRoot = false;
    CC_DequeConf conf;
    cc_deque_conf_init(&conf);
    conf.mem_alloc = custom_alloc;
    conf.mem_calloc = custom_calloc;
    conf.mem_free = custom_free;
    cc_deque_new_conf(&conf, &path->directories);
}

void stringToPath(Path *path, char *strPath)
{
    initPath(path);
    char currentDir[PATH_MAX];
    int strIndex = 0;
    int dirWriteIndex = 0;

    int strLen = (int)strlen(strPath);
    if (strLen > 0 && strPath[0] == PATH_SEP)
    {
        path->isRoot = true;
        strIndex++;
    }
    for (; strIndex < strLen + 1; strIndex++)
    {
        char c = strPath[strIndex];
        if (c == PATH_SEP || c == 0) // end of string or a path separator, append current to queue
        {
            currentDir[dirWriteIndex] = 0;
            cc_deque_add_last(path->directories, custom_strdup(currentDir));
            dirWriteIndex = 0;

            if (strIndex + 1 >= strLen)
            {
                break;
            }

            continue;
        }
        currentDir[dirWriteIndex] = c;
        dirWriteIndex++;
    }
}
void commonRoot(Path *output, Path *path1, Path *path2)
{
    initPath(output);
    output->isRoot = path1->isRoot || path2->isRoot;

    CC_DequeZipIter zipIter;
    cc_deque_zip_iter_init(&zipIter, path1->directories, path2->directories);

    char *currentStr1;
    char *currentStr2;

    while (cc_deque_zip_iter_next(&zipIter, (void **)&currentStr1, (void **)&currentStr2) != CC_ITER_END)
    {
        if (strcmp(currentStr1, currentStr2) != 0)
        {
            break;
        }
        cc_deque_add_last(output->directories, currentStr1);
    }
}
void concatenatePath(Path *prefix, Path *rest)
{
    rest->isRoot = prefix->isRoot;
    cc_deque_reverse(prefix->directories);
    CC_DequeIter iter;
    cc_deque_iter_init(&iter,prefix->directories);
    void *current;
    while (cc_deque_iter_next(&iter, &current) != CC_ITER_END)
    {
        cc_deque_add_first(rest->directories, current);
    }
}

void pathToStr(Path *path, char *outBuffer)
{
    outBuffer[0] = 0;
    char *currentDir;
    CC_DequeIter iter;
    cc_deque_iter_init(&iter, path->directories);

    while (cc_deque_iter_next(&iter, (void **)&currentDir) != CC_ITER_END)
    {
        if ((iter.index-1 > 0) || (iter.index-1 == 0 && path->isRoot))
        {
            strcat(outBuffer, PATH_SEP_STR);
        }
        strcat(outBuffer, currentDir);
    }
}

void sanitizePath(Path* path){
    CC_DequeIter iter;
    cc_deque_iter_init(&iter, path->directories);

    char * str;
    enum cc_stat stat;
    while ((stat = cc_deque_iter_next(&iter, (void**)&str)) != CC_ITER_END){
        if (!strcmp(str, "..")){
            cc_deque_iter_remove(&iter, (void**)&str);
        }
        if (!strcmp(str, ".")){
            cc_deque_iter_remove(&iter, (void**)&str);
        }
    }
}


void addToPath(Path *path, char *dirname)
{
    cc_deque_add_last(path->directories, custom_strdup(dirname));
}

void popFromPath(Path *path)
{
    void **out;
    cc_deque_remove_first(path->directories, out);
}

void removePrefix(Path *path, Path *prefix)
{
    Path newPrefix;
    newPrefix.isRoot = true;
    cc_deque_copy_shallow(prefix->directories, &newPrefix.directories);

    CC_DequeZipIter zipIter;
    cc_deque_zip_iter_init(&zipIter, path->directories, newPrefix.directories);

    char currentStr1[PATH_MAX];
    char currentStr2[PATH_MAX];

    void **out1 = (void **)&currentStr1;
    void **out2 = (void **)&currentStr2;

    while (cc_deque_zip_iter_next(&zipIter, out1, out2) != CC_ITER_END)
    {
        cc_deque_zip_iter_remove(&zipIter, out1, out2);
    }
}
