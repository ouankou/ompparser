#pragma omp target simd safelen(13) map(tofrom: A[0:1024])
