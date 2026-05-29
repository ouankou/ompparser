#pragma omp target parallel loop collapse(3) lastprivate(i, j, k) map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
