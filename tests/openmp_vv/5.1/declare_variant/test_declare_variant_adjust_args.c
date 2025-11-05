#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant(add_dispatch) match(construct={dispatch}) adjust_args(need_device_ptr:arr)
#pragma omp target parallel for
#pragma omp target enter data map(to:arr[0:1024])
#pragma omp dispatch
#pragma omp target parallel for map(tofrom: errors)
#pragma omp target exit data map(delete:arr[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
