#pragma omp declare variant(add_dispatch) match(construct={dispatch}) adjust_args(need_device_ptr:arr)
