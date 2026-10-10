#include "rt.h"

Value f_summer(Value v_inbox_0, Value v_outbox_1);
Value spawn_f_summer(Value *a);

Value f_summer(Value v_inbox_0, Value v_outbox_1) {
    Value v_total_2 = rt_int(0);
    Value v_n_3 = rt_recv(rt_borrow(&v_inbox_0, "inbox"));
    while (rt_truthy(rt_ge(rt_borrow(&v_n_3, "n"), rt_int(0)))) {
        { Value t = rt_add(rt_borrow(&v_total_2, "total"), rt_borrow(&v_n_3, "n")); rt_drop(&v_total_2); v_total_2 = t; }
        { Value t = rt_recv(rt_borrow(&v_inbox_0, "inbox")); rt_drop(&v_n_3); v_n_3 = t; }
    }
    { Value t = rt_send(rt_borrow(&v_outbox_1, "outbox"), rt_move(&v_total_2, "total")); rt_drop(&t); }
    rt_drop(&v_n_3);
    rt_drop(&v_total_2);
    rt_drop(&v_outbox_1);
    rt_drop(&v_inbox_0);
    return rt_int(0);
}

Value spawn_f_summer(Value *a) {
    (void)a;
    return f_summer(a[0], a[1]);
}

int main(void) {
    Value v_inbox_4 = rt_channel();
    Value v_outbox_5 = rt_channel();
    rt_spawn(spawn_f_summer, 2, (Value[]){rt_share(rt_borrow(&v_inbox_4, "inbox")), rt_share(rt_borrow(&v_outbox_5, "outbox"))});
    Value v_i_6 = rt_int(1);
    while (rt_truthy(rt_le(rt_borrow(&v_i_6, "i"), rt_int(5)))) {
        { Value t = rt_send(rt_borrow(&v_inbox_4, "inbox"), rt_move(&v_i_6, "i")); rt_drop(&t); }
        { Value t = rt_add(rt_borrow(&v_i_6, "i"), rt_int(1)); rt_drop(&v_i_6); v_i_6 = t; }
    }
    { Value t = rt_send(rt_borrow(&v_inbox_4, "inbox"), rt_sub(rt_int(0), rt_int(1))); rt_drop(&t); }
    { Value t = rt_recv(rt_borrow(&v_outbox_5, "outbox")); rt_print(t); rt_drop(&t); }
    rt_drop(&v_i_6);
    rt_drop(&v_outbox_5);
    rt_drop(&v_inbox_4);
    rt_join_all();
    return 0;
}

