#pragma omp simd simdlen(64) if(k == 1024)
#pragma omp simd simdlen(64) if(k != 1024)
