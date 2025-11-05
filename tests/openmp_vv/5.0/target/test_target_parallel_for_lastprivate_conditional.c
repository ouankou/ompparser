#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel for lastprivate(conditional: x) map(tofrom: x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
