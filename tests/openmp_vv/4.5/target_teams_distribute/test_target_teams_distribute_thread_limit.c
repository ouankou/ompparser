#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute thread_limit(4) map(from: num_threads)
#pragma omp parallel
#pragma omp target map (from: _ompvv_isOffloadingOn)
