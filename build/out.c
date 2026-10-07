#include "rt.h"

Value f_show(Value v_x_0);

Value f_show(Value v_x_0) {
    rt_print(rt_use(&v_x_0, "x"));
    rt_drop(&v_x_0);
    return rt_int(0);
}

int main(void) {
    Value v_s_1 = rt_str("loop");
    Value v_i_2 = rt_int(0);
    while (rt_truthy(rt_lt(rt_use(&v_i_2, "i"), rt_int(3)))) {
        { Value t = f_show(rt_borrow(&v_s_1, "s")); rt_drop(&t); }
        { Value t = rt_add(rt_use(&v_i_2, "i"), rt_int(1)); rt_drop(&v_i_2); v_i_2 = t; }
    }
    rt_drop(&v_i_2);
    rt_drop(&v_s_1);
    return 0;
}

