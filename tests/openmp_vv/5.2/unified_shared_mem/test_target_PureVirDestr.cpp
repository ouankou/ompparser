#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires unified_shared_memory
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target device(dev) map(tofrom: Errs)
