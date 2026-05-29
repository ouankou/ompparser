#pragma omp target simd simdlen(64) if(k == 1024)
#pragma omp target simd simdlen(64) if(k != 1024)
#pragma omp target map (from: _ompvv_isOffloadingOn)
