#ifndef RT_H
#define RT_H

#include <stdio.h>

typedef enum { T_INT, T_STR, T_FILE, T_LIST, T_CHAN, T_MOVED } Tag;

typedef struct List List;
typedef struct Chan Chan;

typedef struct {
    Tag tag;
    int borrowed;
    union { long i; char *s; FILE *f; List *l; Chan *c; } as;
} Value;

struct List {
    Value *items;
    long len, cap;
};

Value rt_int(long n);
Value rt_str(const char *s);
Value rt_use(const Value *v, const char *name);
Value rt_move(Value *v, const char *name);
Value rt_borrow(const Value *v, const char *name);
void  rt_drop(Value *v);
Value rt_add(Value a, Value b);
Value rt_sub(Value a, Value b);
Value rt_mul(Value a, Value b);
Value rt_div(Value a, Value b);
Value rt_lt(Value a, Value b);
Value rt_gt(Value a, Value b);
Value rt_le(Value a, Value b);
Value rt_ge(Value a, Value b);
Value rt_eq(Value a, Value b);
Value rt_ne(Value a, Value b);
Value rt_open(Value path, Value mode);
Value rt_read(Value f);
Value rt_write(Value f, Value s);
Value rt_close(Value f);
Value rt_len(Value s);
Value rt_trim(Value s);
Value rt_tostr(Value x);
Value rt_list_lit(long n, const Value *items);
Value rt_index(Value list, Value idx);
Value rt_list_set(Value list, Value idx, Value x);
Value rt_push(Value list, Value x);
long rt_list_len(const Value *list);
Value rt_list_get(const Value *list, long i);
Value rt_channel(void);
Value rt_share(Value ch);
Value rt_send(Value ch, Value x);
Value rt_recv(Value ch);
void rt_spawn(Value (*fn)(Value *), int n, const Value *args);
void rt_join_all(void);
int   rt_truthy(Value v);
void  rt_print(Value v);
void  rt_panic(const char *msg);

#endif
