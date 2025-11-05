#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel shared(total) map(tofrom : total)
#pragma omp scope
#pragma omp for
#pragma omp atomic update
#pragma omp target map (from: _ompvv_isOffloadingOn)
