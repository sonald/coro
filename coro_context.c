/**
 * coroutine with context v2
 * example: cc coro_context.c -o demo && ./demo  calc "12 21*100-"
 */

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>

#define emit(d) fprintf(stdout, "%d ", (d))

typedef void (*coro_func_t)(void*);

enum {
    CO_READY,
    CO_RUNNING,
    CO_DEAD,
};

struct coro_ {
    ucontext_t env;
    void* arg;
    coro_func_t func;
    struct coro_* next;

    int state;
};

static struct coro_ pool[10];
static int pool_ptr = 0;
static struct coro_ *head = NULL;

struct scheduler {
    ucontext_t env; /*scheduler context*/
    struct coro_* current;
} sched = { 
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

#define STACK_SIZE  (1<<20)
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


/* demo */
static void gen_even(int limit)
{
    for (int i = 0; i < limit; i++) {
        if (i % 2 == 0) {
            emit(i);
            coro_yield();
        }
    }
}

static void gen_odd(int limit)
{
    for (int i = 0; i < limit; i++) {
        if (i % 2 == 1) {
            emit(i);
            coro_yield();
        }
    }
}


static void gen_third(int limit)
{
    for (int i = 0; i < limit; i++) {
        if (i % 3 == 0) {
            emit(i + limit);
            coro_yield();
        }
    }
}

static void symmetric_test()
{
    coro_create((coro_func_t)gen_even, (void*)11);
    coro_create((coro_func_t)gen_odd, (void*)11);
    coro_create((coro_func_t)gen_third, (void*)11);

    coro_dump();
    coro_schedule();
    fprintf(stdout, "\n");
}

/* RW co-operate */

struct data {
    enum {
        OP,
        NUM,
        EOP
    } t;
    int v;
} d;

static void decode(const char* stream)
{
    const char* p = stream;
    char num[10];

    while (*p) {
        while (isspace(*p)) p++;
        if (isdigit(*p)) {
            int i = 0;
            do {
            num[i++] = *p++;
            } while (isdigit(*p));
            num[i] = 0;

            d.t = NUM;
            d.v = atoi(num);

            printf("emit NUM %d\n", d.v);
            coro_yield();

        } else if (strchr("+-*/", *p)) {
            d.t = OP;
            d.v = *p++;
            printf("emit OP %c\n", d.v);
            coro_yield();

        } else {
            printf("emit EOP\n");
            d.t = EOP;
            break;
        }
    }

    printf("emit EOP\n");
    d.t = EOP;
}

static void parse(int* result)
{
    int v1, v2;
    v1 = d.v; coro_yield();
    while (d.t != EOP) {
        v2 = d.v; coro_yield();
        if (d.t != OP) break;
        switch(d.v) {
            case '+': v1 = v1 + v2; break;
            case '-': v1 = v1 - v2; break;
            case '*': v1 = v1 * v2; break;
            case '/': v1 = v1 / v2; break;
        }
        coro_yield();
    }
    *result = v1;
}

static void calc_test(const char* data)
{
    const char* stream = data ? data: "23 46+10*100-50-10*";
    int result = 0;
    coro_create((coro_func_t)decode, (void*)stream);
    coro_create((coro_func_t)parse, (void*)&result);
    coro_schedule();
    printf("result = %d\n", result);
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "calc") == 0) {
        calc_test(argc > 2 ? argv[2]: NULL);
    } else {
        symmetric_test();
    }
    return 0;
}

