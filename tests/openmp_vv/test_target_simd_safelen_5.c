#pragma omp target simd safelen(5) map(tofrom: A[0:1024])
