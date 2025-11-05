#pragma omp target simd simdlen(128) map(tofrom: A[0:1024])
