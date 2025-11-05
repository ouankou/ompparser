#pragma omp target simd simdlen(8) map(tofrom: A[0:1024])
