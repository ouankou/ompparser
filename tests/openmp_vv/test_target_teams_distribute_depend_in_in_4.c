#pragma omp target data map(to: a[0:1024], b[0:1024]) map(tofrom: c[0:1024], d[0:1024])
