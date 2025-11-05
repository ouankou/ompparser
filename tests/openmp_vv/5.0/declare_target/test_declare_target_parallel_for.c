#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp parallel for num_threads(8) shared(num_threads)
#pragma omp end declare target
#pragma omp target map(tofrom: x, y, z, num_threads)
#pragma omp target map (from: _ompvv_isOffloadingOn)
