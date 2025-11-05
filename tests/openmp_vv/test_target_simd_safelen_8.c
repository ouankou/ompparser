#pragma omp target simd safelen(16) map(tofrom: A[0:1024])
