#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel master taskloop num_threads(8) shared(x, y, z, num_threads)
#pragma omp target map (from: _ompvv_isOffloadingOn)
