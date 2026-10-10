#include "rt.h"

Value f_write_text(Value v_path_0, Value v_text_1);
Value spawn_f_write_text(Value *a);
Value f_analyze(Value v_id_5, Value v_path_6, Value v_done_7);
Value spawn_f_analyze(Value *a);

Value f_write_text(Value v_path_0, Value v_text_1) {
    Value v_f_2 = rt_open(rt_borrow(&v_path_0, "path"), rt_str("w"));
    { Value t = ({ Value o_3 = rt_borrow(&v_f_2, "f"); Value o_4 = rt_borrow(&v_text_1, "text"); rt_write(o_3, o_4); }); rt_drop(&t); }
    { Value t = rt_close(rt_move(&v_f_2, "f")); rt_drop(&t); }
    rt_drop(&v_f_2);
    rt_drop(&v_text_1);
    rt_drop(&v_path_0);
    return rt_int(0);
}

Value spawn_f_write_text(Value *a) {
    (void)a;
    return f_write_text(a[0], a[1]);
}

Value f_analyze(Value v_id_5, Value v_path_6, Value v_done_7) {
    Value v_f_8 = rt_open(rt_borrow(&v_path_6, "path"), rt_str("r"));
    Value v_text_9 = rt_read(rt_move(&v_f_8, "f"));
    Value v_raw_10 = rt_len(rt_borrow(&v_text_9, "text"));
    Value v_trimmed_11 = rt_len(rt_trim(rt_borrow(&v_text_9, "text")));
    Value v_line_16 = rt_add(({ Value o_14 = rt_add(({ Value o_12 = rt_add(rt_borrow(&v_path_6, "path"), rt_str(": raw=")); Value o_13 = rt_tostr(rt_move(&v_raw_10, "raw")); rt_add(o_12, o_13); }), rt_str(" trimmed=")); Value o_15 = rt_tostr(rt_move(&v_trimmed_11, "trimmed")); rt_add(o_14, o_15); }), rt_str("\n"));
    { Value t = ({ Value o_17 = rt_add(rt_add(rt_str("build/stats_"), rt_tostr(rt_move(&v_id_5, "id"))), rt_str(".txt")); Value o_18 = rt_move(&v_line_16, "line"); f_write_text(o_17, o_18); }); rt_drop(&t); }
    { Value t = ({ Value o_19 = rt_borrow(&v_done_7, "done"); Value o_20 = rt_move(&v_raw_10, "raw"); rt_send(o_19, o_20); }); rt_drop(&t); }
    rt_drop(&v_line_16);
    rt_drop(&v_trimmed_11);
    rt_drop(&v_raw_10);
    rt_drop(&v_text_9);
    rt_drop(&v_f_8);
    rt_drop(&v_done_7);
    rt_drop(&v_path_6);
    rt_drop(&v_id_5);
    return rt_int(0);
}

Value spawn_f_analyze(Value *a) {
    (void)a;
    return f_analyze(a[0], a[1], a[2]);
}

int main(void) {
    { Value t = f_write_text(rt_str("build/in_0.txt"), rt_str("  hello world  \n")); rt_drop(&t); }
    { Value t = f_write_text(rt_str("build/in_1.txt"), rt_str("tidy\n")); rt_drop(&t); }
    { Value t = f_write_text(rt_str("build/in_2.txt"), rt_str("\n\n   lots of   padding   \n\n\n")); rt_drop(&t); }
    { Value t = f_write_text(rt_str("build/in_3.txt"), rt_str("x")); rt_drop(&t); }
    Value v_paths_21 = rt_list_lit(4, (Value[]){rt_str("build/in_0.txt"), rt_str("build/in_1.txt"), rt_str("build/in_2.txt"), rt_str("build/in_3.txt")});
    Value v_done_22 = rt_channel();
    Value v_id_23 = rt_int(0);
    {
        Value it_24 = rt_borrow(&v_paths_21, "paths");
        for (long i_24 = 0; i_24 < rt_list_len(&it_24); i_24++) {
            Value v_p_25 = rt_list_get(&it_24, i_24);
            ({ Value o_26 = rt_move(&v_id_23, "id"); Value o_27 = rt_move(&v_p_25, "p"); Value o_28 = rt_share(rt_borrow(&v_done_22, "done")); rt_spawn(spawn_f_analyze, 3, (Value[]){o_26, o_27, o_28}); });
            { Value t = rt_add(rt_borrow(&v_id_23, "id"), rt_int(1)); rt_drop(&v_id_23); v_id_23 = t; }
            rt_drop(&v_p_25);
        }
        rt_drop(&it_24);
    }
    Value v_total_29 = rt_int(0);
    Value v_k_30 = rt_int(0);
    while (rt_truthy(({ Value o_31 = rt_borrow(&v_k_30, "k"); Value o_32 = rt_len(rt_borrow(&v_paths_21, "paths")); rt_lt(o_31, o_32); }))) {
        { Value t = ({ Value o_33 = rt_borrow(&v_total_29, "total"); Value o_34 = rt_recv(rt_borrow(&v_done_22, "done")); rt_add(o_33, o_34); }); rt_drop(&v_total_29); v_total_29 = t; }
        { Value t = rt_add(rt_borrow(&v_k_30, "k"), rt_int(1)); rt_drop(&v_k_30); v_k_30 = t; }
    }
    Value v_report_35 = rt_str("");
    Value v_i_36 = rt_int(0);
    while (rt_truthy(({ Value o_37 = rt_borrow(&v_i_36, "i"); Value o_38 = rt_len(rt_borrow(&v_paths_21, "paths")); rt_lt(o_37, o_38); }))) {
        Value v_f_39 = rt_open(rt_add(rt_add(rt_str("build/stats_"), rt_tostr(rt_move(&v_i_36, "i"))), rt_str(".txt")), rt_str("r"));
        { Value t = ({ Value o_40 = rt_borrow(&v_report_35, "report"); Value o_41 = rt_read(rt_move(&v_f_39, "f")); rt_add(o_40, o_41); }); rt_drop(&v_report_35); v_report_35 = t; }
        { Value t = rt_add(rt_borrow(&v_i_36, "i"), rt_int(1)); rt_drop(&v_i_36); v_i_36 = t; }
        rt_drop(&v_f_39);
    }
    { Value t = f_write_text(rt_str("build/report.txt"), rt_move(&v_report_35, "report")); rt_drop(&t); }
    Value v_rf_42 = rt_open(rt_str("build/report.txt"), rt_str("r"));
    Value v_text_43 = rt_read(rt_move(&v_rf_42, "rf"));
    { Value t = rt_trim(rt_borrow(&v_text_43, "text")); rt_print(t); rt_drop(&t); }
    { Value t = rt_add(rt_str("total bytes: "), rt_tostr(rt_move(&v_total_29, "total"))); rt_print(t); rt_drop(&t); }
    rt_drop(&v_text_43);
    rt_drop(&v_rf_42);
    rt_drop(&v_i_36);
    rt_drop(&v_report_35);
    rt_drop(&v_k_30);
    rt_drop(&v_total_29);
    rt_drop(&v_id_23);
    rt_drop(&v_done_22);
    rt_drop(&v_paths_21);
    rt_join_all();
    return 0;
}

