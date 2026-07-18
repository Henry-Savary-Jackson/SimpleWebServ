#pragma once

#include "cc_array.h"
#include "utils.h"
#include <bits/pthreadtypes.h>
#include <stdatomic.h>

typedef struct {
    int maxWorkers;
    CC_Array* workers;
    pthread_mutex_t queueMutex;
    pthread_cond_t avaialableSignal;
    bool running;
    Queue taskQueue;
}
ThreadPool;

typedef struct{
    void (*callback)(void* );
    void * args;
} Task;

void initThreadPool(ThreadPool* pool,int maxWorkers);
void startThreadPool(ThreadPool* pool);
void submitTask(ThreadPool* pool, void function (void* ), void* args );
void shutDownThreadPool(ThreadPool* pool);
void freeThreadPool(ThreadPool* pool);
