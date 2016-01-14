#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "coro.h"

#define STACK_SIZE  (1<<20)
#define DEFAULT_CAP 10

#define coro_error(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

struct scheduler {
    /* private usage */
    ucontext_t env; /*scheduler context*/
    struct coro_* current;

    struct coro_ pool[DEFAULT_CAP];
    int pool_ptr;
    struct coro_ *head;
};

static struct scheduler * current_sched = NULL;

void coro_dump()
{
    fprintf(stderr, "co list:\n");
    struct coro_ *co = current_sched->head;
    while (co) {
        fprintf(stderr, "co(%p) func %p next %p\n", co, co->func, co->next);
        co = co->next;
    }
}

void coro_destroy(struct coro_* co)
{
    struct coro_** pp = &current_sched->head;
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
    struct coro_* co = &current_sched->pool[current_sched->pool_ptr++];
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
    co->env.uc_link = &current_sched->env;

    uintptr_t ul = (uintptr_t)co;
    makecontext(&co->env, (void (*)())coro_wrapper, 2, (uint32_t)(ul>>32), ul);

    struct coro_** pp = &current_sched->head;
    while (*pp) {
        pp = &(*pp)->next;
    }
    *pp = co;
    return co;
}

void coro_yield()
{
    if (current_sched->current) {
        swapcontext(&current_sched->current->env, &current_sched->env);
    }
}
 
void coro_schedule()
{
    if (!current_sched->head) return;

    while (current_sched->head) {
        if (current_sched->current) {
            struct coro_* next = current_sched->current->next;
            if (!next) next = current_sched->head;

            current_sched->current = next;
        } else {
            current_sched->current = current_sched->head;
        }

        current_sched->current->state = CO_RUNNING;
        swapcontext(&current_sched->env, &current_sched->current->env);

        if (current_sched->current->state == CO_DEAD) {
            free(current_sched->current->env.uc_stack.ss_sp);
            current_sched->current = NULL;
        }
    }
}

void coro_start()
{
    if (current_sched) {
        coro_error("there is a scheduler running\n");
        return;
    }

    struct scheduler* sched = (struct scheduler*)malloc(sizeof(*current_sched));
    memset(sched, 0, sizeof *sched);

    current_sched = sched;
}

void coro_finish()
{
    assert(current_sched != NULL);
    free(current_sched);
    current_sched = NULL;
}

