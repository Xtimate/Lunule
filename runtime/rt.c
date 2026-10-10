#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include "rt.h"

static int owns_heap(Tag t) { return t == T_STR || t == T_FILE || t == T_LIST || t == T_CHAN; }
static void chan_release(Chan *c);
static void list_free(List *l);
static void print_inner(const Value *v);

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

Value rt_use(const Value *v, const char *name) {
    if (v->tag == T_MOVED) panic_moved(name);
    return *v;
}

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

void rt_drop(Value *v) {
    if (!v->borrowed) {
        if (v->tag == T_STR)        free(v->as.s);
        else if (v->tag == T_FILE)  fclose(v->as.f);
        else if (v->tag == T_LIST)  list_free(v->as.l);
        else if (v->tag == T_CHAN)  chan_release(v->as.c);
    }
    v->tag = T_MOVED;
}

Value rt_add(Value a, Value b) {
    if (a.tag == T_INT && b.tag == T_INT) return rt_int(a.as.i + b.as.i);
    if (a.tag == T_STR && b.tag == T_STR) {
        size_t la = strlen(a.as.s), lb = strlen(b.as.s);
        char *buf = malloc(la + lb + 1);
        if (!buf) rt_panic("out of memory");
        memcpy(buf, a.as.s, la);
        memcpy(buf + la, b.as.s, lb + 1);
        rt_drop(&a);
        rt_drop(&b);
        Value v;
        v.tag = T_STR;
        v.borrowed = 0;
        v.as.s = buf;
        return v;
    }
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
        if (a.tag == T_STR && b.tag == T_STR) {               \
            int c = strcmp(a.as.s, b.as.s);                   \
            rt_drop(&a);                                      \
            rt_drop(&b);                                      \
            return rt_int(c OP 0);                            \
        }                                                     \
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
        case T_LIST:  print_inner(&v); printf("\n"); break;
        case T_CHAN:  printf("<channel>\n"); break;
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

Value rt_len(Value s) {
    long n = 0;
    if (s.tag == T_STR) n = (long)strlen(s.as.s);
    else if (s.tag == T_LIST) n = s.as.l->len;
    else rt_panic("len() needs a string or a list");
    rt_drop(&s);
    return rt_int(n);
}

Value rt_trim(Value s) {
    if (s.tag != T_STR) rt_panic("trim() needs a string");
    const char *start = s.as.s;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = s.as.s + strlen(s.as.s);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - start);
    char *buf = malloc(n + 1);
    if (!buf) rt_panic("out of memory");
    memcpy(buf, start, n);
    buf[n] = '\0';
    rt_drop(&s);
    Value v;
    v.tag = T_STR;
    v.borrowed = 0;
    v.as.s = buf;
    return v;
}

Value rt_tostr(Value x) {
    if (x.tag == T_INT) {
        char tmp[32];
        snprintf(tmp, sizeof tmp, "%ld", x.as.i);
        return rt_str(tmp);
    }
    if (x.tag == T_STR) {
        Value out = rt_str(x.as.s);
        rt_drop(&x);
        return out;
    }
    rt_panic("tostr() needs an int or a string");
    return rt_int(0);
}

static void list_free(List *l) {
    for (long i = 0; i < l->len; i++) rt_drop(&l->items[i]);
    free(l->items);
    free(l);
}

static void print_inner(const Value *v) {
    switch(v->tag) {
        case T_INT: printf("%ld", v->as.i); break;
        case T_STR: printf("\"%s\"", v->as.s); break;
        case T_FILE: printf("<file>"); break;
        case T_MOVED: printf("<moved>"); break;
        case T_CHAN: printf("<channel>"); break;
        case T_LIST:
            printf("[");
            for (long i = 0; i < v->as.l->len; i++) {
                if (i) printf(", ");
                print_inner(&v->as.l->items[i]);
            }
            printf("]");
            break;
    }
}

static List *list_new(long cap) {
    List *l = malloc(sizeof *l);
    if (!l) rt_panic("out of memory");
    l->len = 0;
    l->cap = cap < 4 ? 4 : cap;
    l->items = malloc(sizeof(Value) * (size_t)l->cap);
    if (!l->items) rt_panic("out of memory");
    return l;
}

static Value to_elem(Value x) {
    if (x.tag == T_INT) return x;
    if (x.tag == T_STR) {
        Value c = rt_str(x.as.s);
        rt_drop(&x);
        return c;
    }
    rt_panic("lists can only hold ints and strings");
    return x;
}

static Value copy_elem(const Value *e) {
    if (e->tag == T_INT) return rt_int(e->as.i);
    return rt_str(e->as.s);
}

static List *need_list(const Value *v, const char *what) {
    if (v->tag != T_LIST) {
        fprintf(stderr, "runtime error: %s needs a list\n", what);
        exit(1);
    }
    return v->as.l;
}

static long need_index(const List *l, Value idx) {
    if (idx.tag != T_INT) rt_panic("list index must be an int");
    long i = idx.as.i;
    if (i < 0 || i >= l->len) {
        fprintf(stderr, "runtime error: index %ld out of range (length %ld)\n", i, l->len);
        exit(1);
    }
    return i;
}

Value rt_list_lit(long n, const Value *items) {
    List *l = list_new(n);
    for (long i = 0; i < n; i++) l->items[l->len++] = to_elem(items[i]);
    Value v;
    v.tag = T_LIST;
    v.borrowed = 0;
    v.as.l = l;
    return v;
}

Value rt_index(Value list, Value idx) {
    List *l = need_list(&list, "indexing");
    long i = need_index(l, idx);
    Value out = copy_elem(&l->items[i]);
    rt_drop(&list);
    rt_drop(&idx);
    return out;
}

Value rt_list_set(Value list, Value idx, Value x) {
    List *l = need_list(&list, "index assignment");
    long i = need_index(l, idx);
    Value elem = to_elem(x);
    rt_drop(&l->items[i]);
    l->items[i] = elem;
    rt_drop(&list);
    return rt_int(0);
}

Value rt_push(Value list, Value x) {
    List *l = need_list(&list, "push()");
    Value elem = to_elem(x);
    if (l->len == l->cap) {
        long ncap = l->cap * 2;
        Value *bigger = realloc(l->items, sizeof(Value) * (size_t)ncap);
        if (!bigger) rt_panic("out of memory");
        l->items = bigger;
        l->cap = ncap;
    }
    l->items[l->len++] = elem;
    rt_drop(&list);
    return rt_int(0);
}

long rt_list_len(const Value *list) {
    return need_list(list, "for")->len;
}

Value rt_list_get(const Value *list, long i) {
    return copy_elem(&need_list(list, "for")->items[i]);
}

struct Chan {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    Value *buf;
    long head, len, cap;
    int refs;
};

static void chan_release(Chan *c) {
    pthread_mutex_lock(&c->mu);
    int left = --c->refs;
    pthread_cond_broadcast(&c->cv);
    pthread_mutex_unlock(&c->mu);
    if (left == 0) {
        for (long i = 0; i < c->len; i++) rt_drop(&c->buf[(c->head + i) % c->cap]);
        free(c->buf);
        pthread_mutex_destroy(&c->mu);
        pthread_cond_destroy(&c->cv);
        free(c);
    }
}

static Chan *need_chan(const Value *v, const char *what) {
    if (v->tag != T_CHAN) {
        fprintf(stderr, "runtime error: %s needs a channel\n", what);
        exit(1);
    }
    return v->as.c;
}

static Value chan_value(Chan *c) {
    Value v;
    v.tag = T_CHAN;
    v.borrowed = 0;
    v.as.c = c;
    return v;
}

Value rt_channel(void) {
    Chan *c = malloc(sizeof *c);
    if (!c) rt_panic("out of memory");
    pthread_mutex_init(&c->mu, NULL);
    pthread_cond_init(&c->cv, NULL);
    c->cap = 8;
    c->buf = malloc(sizeof(Value) * (size_t)c->cap);
    if (!c->buf) rt_panic("out of memory");
    c->head = 0;
    c->len = 0;
    c->refs = 1;
    return chan_value(c);
}

Value rt_share(Value ch) {
    Chan *c = need_chan(&ch, "share()");
    pthread_mutex_lock(&c->mu);
    c->refs++;
    pthread_mutex_unlock(&c->mu);
    rt_drop(&ch);
    return chan_value(c);
}

static Value msg_take(Value x) {
    switch (x.tag) {
        case T_INT:
            return x;
        case T_STR:
            if (x.borrowed) return rt_str(x.as.s);
            return x;
        case T_FILE:
        case T_LIST:
        case T_CHAN:
            if (x.borrowed) rt_panic("cannot send a borrowed value; move it into send() instead");
            return x;
        default:
            rt_panic("cannot send a dead value");
    }
    return x;
}

Value rt_send(Value ch, Value x) {
    Chan *c = need_chan(&ch, "send()");
    Value m = msg_take(x);
    pthread_mutex_lock(&c->mu);
    if (c->len == c->cap) {
        long ncap = c->cap * 2;
        Value *nb = malloc(sizeof(Value) * (size_t)ncap);
        if (!nb) rt_panic("out of memory");
        for (long i = 0; i < c->len; i++) nb[i] = c->buf[(c->head + i) % c->cap];
        free(c->buf);
        c->buf = nb;
        c->head = 0;
        c->cap = ncap;
    }
    c->buf[(c->head + c->len) % c->cap] = m;
    c->len++;
    pthread_cond_signal(&c->cv);
    pthread_mutex_unlock(&c->mu);
    rt_drop(&ch);
    return rt_int(0);
}

Value rt_recv(Value ch) {
    Chan *c = need_chan(&ch, "recv()");
    pthread_mutex_lock(&c->mu);
    while (c->len == 0) {
        if (c->refs <= 1) {
            pthread_mutex_unlock(&c->mu);
            rt_panic("recv() on an empty channel with no senders left (it would wait forever)");
        }
        pthread_cond_wait(&c->cv, &c->mu);
    }
    Value m = c->buf[c->head];
    c->head = (c->head + 1) % c->cap;
    c->len--;
    pthread_mutex_unlock(&c->mu);
    rt_drop(&ch);
    return m;
}

typedef struct {
    Value (*fn)(Value *);
    Value *args;
} SpawnArgs;

static pthread_mutex_t threads_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t *threads = NULL;
static long nthreads = 0, thread_cap = 0;

static void *thread_main(void *p) {
    SpawnArgs *sa = p;
    Value r = sa->fn(sa->args);
    rt_drop(&r);
    free(sa->args);
    free(sa);
    return NULL;
}

void rt_spawn(Value (*fn)(Value *), int n, const Value *args) {
    SpawnArgs *sa = malloc(sizeof *sa);
    if (!sa) rt_panic("out of memory");
    sa->fn = fn;
    sa->args = malloc(sizeof(Value) * (size_t)(n > 0 ? n : 1));
    if (!sa->args) rt_panic("out of memory");
    for (int i = 0; i < n; i++) sa->args[i] = args[i];
    pthread_t t;
    if (pthread_create(&t, NULL, thread_main, sa) != 0) rt_panic("could not start a thread");
    pthread_mutex_lock(&threads_mu);
    if (nthreads == thread_cap) {
        thread_cap = thread_cap ? thread_cap * 2 : 8;
        threads = realloc(threads, sizeof(pthread_t) * (size_t)thread_cap);
        if (!threads) rt_panic("out of memory");
    }
    threads[nthreads++] = t;
    pthread_mutex_unlock(&threads_mu);
}

void rt_join_all(void) {
    for (;;) {
        pthread_mutex_lock(&threads_mu);
        if (nthreads == 0) {
            pthread_mutex_unlock(&threads_mu);
            break;
        }
        pthread_t t = threads[--nthreads];
        pthread_mutex_unlock(&threads_mu);
        pthread_join(t, NULL);
    }
    free(threads);
    threads = NULL;
    thread_cap = 0;
}
