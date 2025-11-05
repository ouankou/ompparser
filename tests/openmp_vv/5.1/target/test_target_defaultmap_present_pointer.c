#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: ptr)
#pragma omp target map(tofrom: errors) defaultmap(present: pointer)
#pragma omp target exit data map(delete: ptr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
