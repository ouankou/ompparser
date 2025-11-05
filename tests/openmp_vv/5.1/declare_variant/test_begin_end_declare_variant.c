#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare variant match(construct={parallel})
#pragma omp for
#pragma omp end declare variant
#pragma omp begin declare variant match(construct={target})
#pragma omp for
#pragma omp end declare variant
#pragma omp parallel
#pragma omp target map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
