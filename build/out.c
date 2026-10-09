#include "rt.h"

int main(void) {
    Value v_n_0 = rt_int(0);
    Value v_acc_1 = rt_str("");
    while (rt_truthy(rt_lt(rt_borrow(&v_n_0, "n"), rt_int(3)))) {
        { Value t = rt_add(rt_borrow(&v_acc_1, "acc"), rt_tostr(rt_move(&v_n_0, "n"))); rt_drop(&v_acc_1); v_acc_1 = t; }
        { Value t = rt_add(rt_borrow(&v_n_0, "n"), rt_int(1)); rt_drop(&v_n_0); v_n_0 = t; }
    }
    rt_print(rt_use(&v_acc_1, "acc"));
    rt_drop(&v_acc_1);
    rt_drop(&v_n_0);
    return 0;
}

