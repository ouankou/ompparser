#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel num_threads(8) shared(x, y, z, num_threads) map(tofrom: x, num_threads) map(to: y, z)
#pragma omp master taskloop simd
#pragma omp target map (from: _ompvv_isOffloadingOn)
