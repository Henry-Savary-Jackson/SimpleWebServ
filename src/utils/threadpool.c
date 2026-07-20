#include "cc_array.h"
#include "cc_common.h"
#include "cc_deque.h"
#include "memory/cc_dynamic_pool.h"
#include "utils.h"
#include <assert.h>
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

        while (pool->running && !cc_deque_size(pool->taskQueue))
        {
            pthread_cond_wait(&pool->avaialableSignal, &pool->queueMutex);
        }

        if (!pool->running)
        {
            pthread_mutex_unlock(&pool->queueMutex);
            return NULL;
        }

        Task *t;
        enum cc_stat result = cc_deque_remove_first(pool->taskQueue,(void**)&t);
        assert(result == CC_OK);

        pthread_mutex_unlock(&pool->queueMutex);
        t->callback(t->args);
        free(t);
    }

    pthread_mutex_unlock(&pool->queueMutex);
    return NULL;
}

void initThreadPool(ThreadPool *pool, int maxWorkers, CC_DynamicPool* arena)
{
    pool->maxWorkers = maxWorkers;
    pool->running = false;
    pool->arena = arena;
    pthread_mutex_init(&pool->queueMutex, NULL);
    pthread_cond_init(&pool->avaialableSignal, NULL);

    CC_ArrayConf confArr;

    cc_array_conf_init(&confArr);
    confArr.pool = arena;
    cc_array_new_conf(&confArr,&pool->workers);

    CC_DequeConf conf;
    cc_deque_conf_init(&conf);
    conf.pool = arena;
    cc_deque_new_conf( &conf, &pool->taskQueue);

}
void submitTask(ThreadPool *pool, void function(void *), void *args)
{
    pthread_mutex_lock(&pool->queueMutex);
    Task *task = malloc(sizeof(Task));
    task->args = args;
    task->callback = function;
    cc_deque_add_last(pool->taskQueue, task);
    pthread_cond_signal(&pool->avaialableSignal);
    pthread_mutex_unlock(&pool->queueMutex);
}
void shutDownThreadPool(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->queueMutex);
    pool->running = false;
    // wake up all the current threads
    pthread_cond_broadcast(&pool->avaialableSignal);
    pthread_mutex_unlock(&pool->queueMutex);
    for (int i = 0; i < pool->maxWorkers; i++)
    {
        pthread_t *thread;
        cc_array_get_at(pool->workers, i, (void **)&thread);
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
}

void startThreadPool(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->queueMutex);
    pool->running = true;
    pthread_mutex_unlock(&pool->queueMutex);
    for (int i = 0; i < pool->maxWorkers; i++)
    {
        pthread_t *worker = cc_dynamic_pool_malloc(sizeof(pthread_t), pool->arena);
        pthread_create(worker, NULL, task_handler, pool);
        cc_array_add(pool->workers, worker);
    }
}
