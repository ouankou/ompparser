#pragma omp parallel
#pragma omp for simd simdlen(16) aligned(x,y:64)
#pragma omp for simd simdlen(16) aligned(x,y:64)
#pragma omp target map (from: _ompvv_isOffloadingOn)
