#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rt.h"

void rt_panic(const char *msg) {
    fprintf(stderr, "runtime error: %s\n", msg);
    exit(1);
}

static void panic_moved(const char *name) {
    fprintf(stderr, "runtime error: use of moved value '%s'\n", name);
    exit(1);
}

Value rt_int(long n) {
    Value v;
    v.tag = T_INT;
    v.borrowed = 0;
    v.as.i = n;
    return v;
}

Value rt_str(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) rt_panic("out of memory");
    memcpy(copy, s, len + 1);
    Value v;
    v.tag = T_STR;
    v.borrowed = 0;
    v.as.s = copy;
    return v;
}

/* Borrow: read the value without taking ownership. */
Value rt_use(const Value *v, const char *name) {
    if (v->tag == T_MOVED) panic_moved(name);
    return *v;
}

/* Move: take ownership. Strings leave the source dead; ints are copied. */
Value rt_move(Value *v, const char *name) {
    if (v->tag == T_MOVED) panic_moved(name);
    if (v->tag == T_STR && v->borrowed) {
        fprintf(stderr, "runtime error: cannot move borrowed value '%s'\n", name);
        exit(1);
    }
    Value out = *v;
    if (v->tag == T_STR) v->tag = T_MOVED;
    return out;
}

Value rt_borrow(const Value *v, const char *name) {
    if (v->tag == T_MOVED) panic_moved(name);
    Value out = *v;
    if (out.tag == T_STR) out.borrowed = 1;
    return out;
}

/* Free what the value owns, and mark it dead so a second drop does nothing. */
void rt_drop(Value *v) {
    if (v->tag == T_STR && !v->borrowed) free(v->as.s);
    v->tag = T_MOVED;
}

Value rt_add(Value a, Value b) {
    if (a.tag == T_INT && b.tag == T_INT) return rt_int(a.as.i + b.as.i);
    rt_panic("cannot add these types");
    return rt_int(0);
}

Value rt_sub(Value a, Value b) {
    if (a.tag == T_INT && b.tag == T_INT) return rt_int(a.as.i - b.as.i);
    rt_panic("cannot subtract these types");
    return rt_int(0);
}

Value rt_mul(Value a, Value b) {
    if (a.tag == T_INT && b.tag == T_INT) return rt_int(a.as.i * b.as.i);
    rt_panic("cannot multiply these types");
    return rt_int(0);
}

Value rt_div(Value a, Value b) {
    if (a.tag == T_INT && b.tag == T_INT) {
        if (b.as.i == 0) rt_panic("division by zero");
        return rt_int(a.as.i / b.as.i);
    }
    rt_panic("cannot divide these types");
    return rt_int(0);
}

#define CMP_FN(NAME, OP)                                      \
    Value NAME(Value a, Value b) {                            \
        if (a.tag == T_INT && b.tag == T_INT)                 \
            return rt_int(a.as.i OP b.as.i);                  \
        rt_panic("cannot compare these types");               \
        return rt_int(0);                                     \
    }

CMP_FN(rt_lt, <)
CMP_FN(rt_gt, >)
CMP_FN(rt_le, <=)
CMP_FN(rt_ge, >=)
CMP_FN(rt_eq, ==)
CMP_FN(rt_ne, !=)

    int rt_truthy(Value v) {
        if (v.tag == T_INT) return v.as.i != 0;
        rt_panic("condition must be an int");
        return 0;
    }

void rt_print(Value v) {
    switch (v.tag) {
        case T_INT:   printf("%ld\n", v.as.i); break;
        case T_STR:   printf("%s\n", v.as.s); break;
        case T_MOVED: rt_panic("print of a dead value"); break;
    }
}
