#pragma omp target simd simdlen(64) if(k != 1024)
