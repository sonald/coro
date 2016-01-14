#ifndef _SIAN_CORO_H
#define _SIAN_CORO_H

#include <signal.h>
#include <ucontext.h>

typedef void (*coro_func_t)(void*);

enum coro_state_t {
    CO_READY,
    CO_RUNNING,
    CO_DEAD,
};

struct coro_ {
    ucontext_t env;
    void* arg;
    coro_func_t func;
    struct coro_* next;

    enum coro_state_t state;
};

struct scheduler {
    ucontext_t env; /*scheduler context*/
    struct coro_* current;
};


#ifdef __cplusplus
extern "C" {
#endif

struct coro_* coro_create(coro_func_t fp, void* arg);
void coro_destroy(struct coro_* co);

void coro_yield();
void coro_schedule();

void coro_dump();

#ifdef __cplusplus
}
#endif
#endif

