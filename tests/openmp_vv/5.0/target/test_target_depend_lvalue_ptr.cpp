#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel map(tofrom: *ptr, value)
#pragma omp single
#pragma omp task depend(out: *ptr)
#pragma omp task depend(in: *ptr)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
