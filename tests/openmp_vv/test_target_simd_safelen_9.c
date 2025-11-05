#pragma omp target simd safelen(100) map(tofrom: A[0:1024])
