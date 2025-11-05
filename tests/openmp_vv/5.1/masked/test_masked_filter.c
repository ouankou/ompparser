#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(threads)
#pragma omp atomic read
#pragma omp masked filter(3)
#pragma omp atomic
#pragma omp target map (from: _ompvv_isOffloadingOn)
