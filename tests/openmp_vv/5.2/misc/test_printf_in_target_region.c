#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: integer, floater, doubler, single_char, shortie, unsigned_shortie, long_one, unsigned_long, long_long)
