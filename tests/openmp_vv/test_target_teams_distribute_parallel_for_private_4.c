#pragma omp target data map(to: a[0:1024], b[0:1024], c[0:1024]) map(from: d[0:1024])
