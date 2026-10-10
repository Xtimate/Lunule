#include "rt.h"

int main(void) {
    Value v_xs_0 = rt_list_lit(1, (Value[]){rt_list_lit(1, (Value[]){rt_int(1)})});
    rt_drop(&v_xs_0);
    return 0;
}

