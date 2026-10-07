#include "os_thread.h"

#include <stdlib.h>
#include <pthread.h>

struct ptx_os_mutex { pthread_mutex_t m; };
struct ptx_os_cond  { pthread_cond_t  c; };

ptx_os_mutex_t* ptx_os_mutex_create(void)
{
    ptx_os_mutex_t* m = (ptx_os_mutex_t*)malloc(sizeof(*m));
    if (m)
        pthread_mutex_init(&m->m, NULL);
    return m;
}

void ptx_os_mutex_destroy(ptx_os_mutex_t* m)
{
    if (!m) return;
    pthread_mutex_destroy(&m->m);
    free(m);
}

void ptx_os_mutex_lock(ptx_os_mutex_t* m)   { if (m) pthread_mutex_lock(&m->m); }
void ptx_os_mutex_unlock(ptx_os_mutex_t* m) { if (m) pthread_mutex_unlock(&m->m); }

ptx_os_cond_t* ptx_os_cond_create(void)
{
    ptx_os_cond_t* c = (ptx_os_cond_t*)malloc(sizeof(*c));
    if (c)
        pthread_cond_init(&c->c, NULL);
    return c;
}

void ptx_os_cond_destroy(ptx_os_cond_t* c)
{
    if (!c) return;
    pthread_cond_destroy(&c->c);
    free(c);
}

void ptx_os_cond_wait(ptx_os_cond_t* c, ptx_os_mutex_t* m)
{
    if (c && m)
        pthread_cond_wait(&c->c, &m->m);
}

void ptx_os_cond_signal(ptx_os_cond_t* c)
{
    if (c)
        pthread_cond_signal(&c->c);
}

struct ptx_thread_arg { ptx_os_thread_fn fn; void* arg; };

static void* ptx_thread_trampoline(void* p)
{
    struct ptx_thread_arg* ta = (struct ptx_thread_arg*)p;
    ptx_os_thread_fn fn = ta->fn;
    void* arg = ta->arg;
    free(ta);
    fn(arg);
    return NULL;
}

int ptx_os_thread_start(ptx_os_thread_fn fn, void* arg)
{
    struct ptx_thread_arg* ta = (struct ptx_thread_arg*)malloc(sizeof(*ta));
    if (!ta) return -1;
    ta->fn = fn;
    ta->arg = arg;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t tid;
    int r = pthread_create(&tid, &attr, ptx_thread_trampoline, ta);
    pthread_attr_destroy(&attr);
    if (r != 0) {
        free(ta);
        return -1;
    }
    return 0;
}
