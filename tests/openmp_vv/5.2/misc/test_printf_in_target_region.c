#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: integer, floater, doubler, single_char, shortie, unsigned_shortie, long_one, unsigned_long, long_long)
