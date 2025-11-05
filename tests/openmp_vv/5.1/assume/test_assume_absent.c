#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp assume absent(teams, masked, scope, simd)
#pragma omp target parallel for map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
