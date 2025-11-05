#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(s)
#pragma omp for
#pragma omp single
#pragma omp scope reduction(+:s)
#pragma omp target map (from: _ompvv_isOffloadingOn)
