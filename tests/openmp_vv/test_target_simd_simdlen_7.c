#pragma omp target simd simdlen(13) map(tofrom: A[0:1024])
