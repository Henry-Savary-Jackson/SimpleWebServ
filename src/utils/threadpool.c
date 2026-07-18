#include "cc_array.h"
#include "utils.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threadpool.h>

void *task_handler(void *arg)

{
    ThreadPool *pool = (ThreadPool *)arg;
    while (pool->running)
    {
        pthread_mutex_lock(&pool->queueMutex);
        while (pool->running && empty(&pool->taskQueue))
            pthread_cond_wait(&pool->avaialableSignal, &pool->queueMutex);

        if (!pool->running)
        {
            pthread_mutex_unlock(&pool->queueMutex);
            return NULL;
        }

        Task *t = (Task *)pop(&pool->taskQueue);

        pthread_mutex_unlock(&pool->queueMutex);
        t->callback(t->args);
        free(t);
    }

    pthread_mutex_unlock(&pool->queueMutex);
    return NULL;
}

void initThreadPool(ThreadPool *pool, int maxWorkers)
{
    pool->maxWorkers = maxWorkers;
    pool->running = false;
    pthread_mutex_init(&pool->queueMutex, NULL);
    pthread_cond_init(&pool->avaialableSignal, NULL);
    pool->workers = NULL;
    initQueue(&pool->taskQueue, sizeof(Task));
}
void submitTask(ThreadPool *pool, void function(void *), void *args)
{
    pthread_mutex_lock(&pool->queueMutex);
    Task *task = malloc(sizeof(Task));
    task->args = args;
    task->callback = function;
    push(&pool->taskQueue, task);
    pthread_cond_signal(&pool->avaialableSignal);
    pthread_mutex_unlock(&pool->queueMutex);
}
void shutDownThreadPool(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->queueMutex);
    pool->running = false;
    pthread_cond_broadcast(&pool->avaialableSignal);
    pthread_mutex_unlock(&pool->queueMutex);
    for (int i = 0; i < pool->maxWorkers; i++)
    {
        pthread_t* thread;
        cc_array_get_at(pool->workers, i, (void**)&thread);
        pthread_cancel(*thread);
        void *status;
        pthread_join(*thread, &status);
        // must free
        free(thread);
    }
}
void freeThreadPool(ThreadPool *pool)
{
    shutDownThreadPool(pool);
    pthread_mutex_lock(&pool->queueMutex);
    freeQueue(&pool->taskQueue);
    pthread_mutex_unlock(&pool->queueMutex);
}

void startThreadPool(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->queueMutex);
    pool->running = true;
    pthread_mutex_unlock(&pool->queueMutex);
    for (int i = 0; i < pool->maxWorkers; i++)
    {
        pthread_t* worker = malloc(sizeof(pthread_t));
        pthread_create(worker, NULL, task_handler, pool);
        cc_array_add(pool->workers, worker);
    }
}
