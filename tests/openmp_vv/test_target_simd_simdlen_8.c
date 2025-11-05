#pragma omp target simd simdlen(16) map(tofrom: A[0:1024])
