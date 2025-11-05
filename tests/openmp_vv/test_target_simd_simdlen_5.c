#pragma omp target simd simdlen(5) map(tofrom: A[0:1024])
