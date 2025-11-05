#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: x, num_threads) map(to: y, z)
#pragma omp parallel master taskloop simd num_threads(8) shared(x, y, z, num_threads)
#pragma omp target map (from: _ompvv_isOffloadingOn)
