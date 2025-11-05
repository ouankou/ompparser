#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(test1, test2)
#pragma omp scope
#pragma omp for
#pragma omp scope nowait
#pragma omp for
#pragma omp target map (from: _ompvv_isOffloadingOn)
