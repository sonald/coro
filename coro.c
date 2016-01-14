#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "coro.h"

#define STACK_SIZE  (1<<20)
#define DEFAULT_CAP 10

static struct coro_ pool[DEFAULT_CAP];
static int pool_ptr = 0;
static struct coro_ *head = NULL;

struct scheduler sched = { 
    0
};

void coro_dump()
{
    fprintf(stderr, "co list:\n");
    struct coro_ *co = head;
    while (co) {
        fprintf(stderr, "co(%p) func %p next %p\n", co, co->func, co->next);
        co = co->next;
    }
}

void coro_destroy(struct coro_* co)
{
    struct coro_** pp = &head;
    while (*pp) {
        if (*pp == co) {
            *pp = (*pp)->next;
            /* stack free should be delayed after routine returned */
            co->state = CO_DEAD;
            break;
        }

        pp = &(*pp)->next;
    }
}

static void coro_wrapper(uint32_t hi, uint32_t lo)
{
    struct coro_* co = (struct coro_*)(((uintptr_t)hi << 32) | lo);
    co->func(co->arg);
    coro_destroy(co);
}

struct coro_* coro_create(coro_func_t fp, void* arg)
{
    struct coro_* co = &pool[pool_ptr++];
    memset(co, 0, sizeof *co);
    co->func = fp;
    co->arg = arg;
    co->state = CO_READY;

    if (getcontext(&co->env) < 0) {
        fprintf(stderr, "%s\n", "getcontext failed");
        return NULL;
    }

    if (posix_memalign(&co->env.uc_stack.ss_sp, 8, STACK_SIZE) != 0) {
        fprintf(stderr, "%s\n", "alloc error");
        return NULL;
    }
    co->env.uc_stack.ss_size = STACK_SIZE;
    co->env.uc_link = &sched.env;

    uintptr_t ul = (uintptr_t)co;
    makecontext(&co->env, (void (*)())coro_wrapper, 2, (uint32_t)(ul>>32), ul);

    struct coro_** pp = &head;
    while (*pp) {
        pp = &(*pp)->next;
    }
    *pp = co;
    return co;
}

void coro_yield()
{
    if (sched.current) {
        swapcontext(&sched.current->env, &sched.env);
    }
}
 
void coro_schedule()
{
    if (!head) return;

    while (head) {
        if (sched.current) {
            struct coro_* next = sched.current->next;
            if (!next) next = head;

            sched.current = next;
        } else {
            sched.current = head;
        }

        sched.current->state = CO_RUNNING;
        swapcontext(&sched.env, &sched.current->env);

        if (sched.current->state == CO_DEAD) {
            free(sched.current->env.uc_stack.ss_sp);
            sched.current = NULL;
        }
    }
}

