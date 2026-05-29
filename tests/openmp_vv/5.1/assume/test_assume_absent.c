#pragma omp assume absent(teams, masked, scope, simd)
#pragma omp target parallel for map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
