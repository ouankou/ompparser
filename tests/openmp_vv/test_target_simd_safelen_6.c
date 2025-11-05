#pragma omp target simd safelen(8) map(tofrom: A[0:1024])
