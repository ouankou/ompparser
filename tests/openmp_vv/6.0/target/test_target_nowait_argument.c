#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target nowait(is_deferred) map(tofrom: x)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
