#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires atomic_default_mem_order(relaxed)
#pragma omp target parallel num_threads(2) map(tofrom: x, y, errors)
#pragma omp flush
#pragma omp atomic write
#pragma omp atomic read
#pragma omp flush
#pragma omp target map (from: _ompvv_isOffloadingOn)
