#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel loop collapse(3) lastprivate(i, j, k) map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
