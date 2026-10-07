#ifndef _PTX_OS_THREAD_H
#define _PTX_OS_THREAD_H

typedef struct ptx_os_mutex ptx_os_mutex_t;
typedef struct ptx_os_cond  ptx_os_cond_t;

typedef void (*ptx_os_thread_fn)(void* arg);

ptx_os_mutex_t* ptx_os_mutex_create(void);
void ptx_os_mutex_destroy(ptx_os_mutex_t*);
void ptx_os_mutex_lock(ptx_os_mutex_t*);
void ptx_os_mutex_unlock(ptx_os_mutex_t*);

ptx_os_cond_t* ptx_os_cond_create(void);
void ptx_os_cond_destroy(ptx_os_cond_t*);
void ptx_os_cond_wait(ptx_os_cond_t*, ptx_os_mutex_t*);
void ptx_os_cond_signal(ptx_os_cond_t*);

/* 启动一个 detached 线程; 返回 0 成功, -1 失败/不支持 */
int ptx_os_thread_start(ptx_os_thread_fn fn, void* arg);

#endif /* _PTX_OS_THREAD_H */
