#pragma omp target simd safelen(1) map(tofrom: A[0:1024])
