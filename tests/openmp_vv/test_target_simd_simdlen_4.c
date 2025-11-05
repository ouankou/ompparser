#pragma omp target simd simdlen(1) map(tofrom: A[0:1024])
