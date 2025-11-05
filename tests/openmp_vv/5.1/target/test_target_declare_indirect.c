#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target to(fun1, fun2, fun3) indirect
#pragma omp target map(from: ret_val)
#pragma omp target map (from: _ompvv_isOffloadingOn)
