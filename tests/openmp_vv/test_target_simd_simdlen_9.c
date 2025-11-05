#pragma omp target simd simdlen(100) map(tofrom: A[0:1024])
