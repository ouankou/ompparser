#pragma omp target simd safelen(128) map(tofrom: A[0:1024])
