#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "rt.h"

static int owns_heap(Tag t) { return t == T_STR || t == T_FILE; }

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
    if (owns_heap(v->tag) && v->borrowed) {
        fprintf(stderr, "runtime error: cannot move borrowed value '%s'\n", name);
        exit(1);
    }
    Value out = *v;
    if (owns_heap(v->tag)) v->tag = T_MOVED;
    return out;
}

Value rt_borrow(const Value *v, const char *name) {
    if (v->tag == T_MOVED) panic_moved(name);
    Value out = *v;
    if (owns_heap(out.tag)) out.borrowed = 1;
    return out;
}

/* Free what the value owns, and mark it dead so a second drop does nothing. */
void rt_drop(Value *v) {
    if (!v->borrowed) {
        if (v->tag == T_STR)        free(v->as.s);
        else if (v->tag == T_FILE)  fclose(v->as.f);
    }
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
        case T_FILE:  printf("<file>\n"); break;
        case T_MOVED: rt_panic("print of a dead value"); break;
    }
}

Value rt_open(Value path, Value mode) {
    if (path.tag != T_STR || mode.tag != T_STR)
        rt_panic("open() needs a path string and a mode string");
    const char *m = mode.as.s;
    if (strcmp(m, "r") != 0 && strcmp(m, "w") != 0 && strcmp(m, "a") != 0)
        rt_panic("open() mode must be \"r\", \"w\" or \"a\"");
    FILE *fp = fopen(path.as.s, m);
    if (!fp) {
        fprintf(stderr, "runtime error: cannot open '%s': %s\n", path.as.s, strerror(errno));
        exit(1);
    }
    rt_drop(&path);
    rt_drop(&mode);
    Value v;
    v.tag = T_FILE;
    v.borrowed = 0;
    v.as.f = fp;
    return v;
}

Value rt_read(Value f) {
    if (f.tag != T_FILE) rt_panic("read() needs a file");
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (!buf) rt_panic("out of memory");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f.as.f)) > 0) {
        len += n;
        if (cap - len < 2) {
            cap *= 2;
            char *bigger = realloc(buf, cap);
            if (!bigger) { free(buf); rt_panic("out of memory"); }
            buf = bigger;
        }
    }
    if (ferror(f.as.f)) { free(buf); rt_panic("read failed (is the file open for reading?)"); }
    buf[len] = '\0';
    rt_drop(&f);
    Value v;
    v.tag = T_STR;
    v.borrowed = 0;
    v.as.s = buf;
    return v;
}

Value rt_write(Value f, Value s) {
    if (f.tag != T_FILE) rt_panic("write() needs a file as its first argument");
    if (s.tag != T_STR)  rt_panic("write() needs a string as its second argument");
    size_t len = strlen(s.as.s);
    if (fwrite(s.as.s, 1, len, f.as.f) != len)
        rt_panic("write failed (is the file open for writing?)");
    rt_drop(&f);
    rt_drop(&s);
    return rt_int((long)len);
}

Value rt_close(Value f) {
    if (f.tag != T_FILE) rt_panic("close() needs a file");
    if (f.borrowed) rt_panic("cannot close a borrowed file");
    rt_drop(&f);
    return rt_int(0);
}
