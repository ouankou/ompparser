#pragma omp target simd nontemporal (a, b, c)
#pragma omp target map (from: _ompvv_isOffloadingOn)
