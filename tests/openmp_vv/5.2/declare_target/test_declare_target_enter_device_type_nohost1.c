#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target enter(fun) device_type(nohost)
#pragma omp declare target enter(a,b,c,i,dev)
#pragma omp declare variant(host_function) match(device={kind(host)})
#pragma omp target update to(a,b,c)
#pragma omp target
#pragma omp target update from (a,b,c,dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
