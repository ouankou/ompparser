#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp declare target
#pragma omp end declare target
#pragma omp end declare target
#pragma omp parallel for
#pragma omp target
#pragma omp target update from(errors, a, b, c)
#pragma omp target update to(a,b,c)
#pragma omp target map (from: _ompvv_isOffloadingOn)
