#pragma omp target simd order(concurrent)
#pragma omp target map (from: _ompvv_isOffloadingOn)
