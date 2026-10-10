#include "rt.h"

Value f_five(void);
Value spawn_f_five(Value *a);

Value f_five(void) {
    {
        Value ret = rt_int(5);
        return ret;
    }
    return rt_int(0);
}

Value spawn_f_five(Value *a) {
    (void)a;
    return f_five();
}

int main(void) {
    { Value t = f_five(); rt_print(t); rt_drop(&t); }
    rt_join_all();
    return 0;
}

