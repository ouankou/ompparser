#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel for map(pmax) shared(pmax)
#pragma omp atomic compare
#pragma omp target map (from: _ompvv_isOffloadingOn)
