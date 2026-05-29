#pragma omp target parallel loop order(concurrent)
#pragma omp target map (from: _ompvv_isOffloadingOn)
