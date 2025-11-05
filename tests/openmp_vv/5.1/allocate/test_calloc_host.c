#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for
#pragma omp atomic write
#pragma omp parallel for
#pragma omp parallel for
#pragma omp atomic write
#pragma omp target map (from: _ompvv_isOffloadingOn)
