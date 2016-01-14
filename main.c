#include "coro.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define emit(d) fprintf(stdout, "%d ", (d))

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
    coro_start();
    coro_create((coro_func_t)gen_even, (void*)11);
    coro_create((coro_func_t)gen_odd, (void*)11);
    coro_create((coro_func_t)gen_third, (void*)11);

    coro_dump();
    coro_schedule();
    fprintf(stdout, "\n");
    coro_finish();
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
    coro_start();
    const char* stream = data ? data: "23 46+10*100-50-10*";
    fprintf(stderr, "calc '%s'\n", stream);
    int result = 0;
    coro_create((coro_func_t)decode, (void*)stream);
    coro_create((coro_func_t)parse, (void*)&result);
    coro_schedule();
    printf("result = %d\n", result);
    coro_finish();
}


int main(int argc, char *argv[])
{
    symmetric_test();
    calc_test(argc > 1 ? argv[1]: NULL);
    return 0;
}

