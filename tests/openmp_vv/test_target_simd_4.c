#pragma omp target simd map(to: b[0:1024], c[0:1024]) map(tofrom: a[0:1024])
