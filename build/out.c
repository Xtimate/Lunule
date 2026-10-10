#include "rt.h"

int main(void) {
    Value v_ch_0 = rt_channel();
    { Value t = rt_send(rt_borrow(&v_ch_0, "ch"), rt_int(1)); rt_drop(&t); }
    { Value t = rt_send(rt_borrow(&v_ch_0, "ch"), rt_int(2)); rt_drop(&t); }
    Value v_xs_3 = ({ Value o_1 = rt_recv(rt_borrow(&v_ch_0, "ch")); Value o_2 = rt_recv(rt_borrow(&v_ch_0, "ch")); rt_list_lit(2, (Value[]){o_1, o_2}); });
    rt_print(rt_use(&v_xs_3, "xs"));
    rt_drop(&v_xs_3);
    rt_drop(&v_ch_0);
    rt_join_all();
    return 0;
}

