#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target defaultmap(none) map(tofrom: scalar, A, new_struct, ptr)
#pragma omp target defaultmap(none) map(to: scalar, A, new_struct, ptr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
