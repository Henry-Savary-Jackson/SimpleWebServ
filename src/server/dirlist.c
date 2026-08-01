#include "http.h"
#include "parser.h"
#include "server.h"
#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

int listDir(char* path, GrowingBuffer* outputBuffer ){
    DIR* dir = opendir(path);
    if (!dir){
        return errno;
    }
    int pathLen = strlen(path);
    struct dirent* entry;
    char entry_path[PATH_MAX];
    strcpy(entry_path, path);

    while ((entry = readdir(dir))){
        if (!strcmp (entry->d_name, ".") || !strcmp(entry->d_name, "..")){
            continue;
        }
        bool isDir = (bool)(entry->d_type & DT_DIR);
        entry_path[pathLen] = 0;
        strcat(entry_path, entry->d_name);

        char* mimetype = isDir? "DIR": getMimeTypeForFile(entry_path);

        struct stat filestat;
        int result = stat((const char*)entry_path, &filestat);
        if (result){
            return -1;
        }
        long time_mod = filestat.st_mtim.tv_sec;

        char line[PATH_MAX];
        int count_chars = snprintf(line, PATH_MAX, "%s:%ld:%s\n",mimetype, time_mod, entry->d_name);
        count_chars = count_chars >= PATH_MAX ? PATH_MAX-1 : count_chars;
        appendGrowingBuffer(outputBuffer, line, count_chars );
    }
    return 0;
}

int handleDirectoryList(char * path, HTTPRequest *request, HTTPResponse *response, FileSystemHandler *handler, int connfd){

    GrowingBuffer buffer;
    initGrowingBuffer(&buffer, 1024);
    response->contentEncoding = IDENTITY_ENCODING;
    response->transferEncoding = IDENTITY_ENCODING;

    int result = listDir(path, &buffer);
    if (result){
        return result;
    }

    setResponseBody(response, buffer.ptr, buffer.size);

    sendResponse(response, connfd);
    return 0;
}
