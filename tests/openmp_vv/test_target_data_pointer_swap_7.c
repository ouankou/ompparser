#pragma omp target data map(tofrom: a[0:1024]) map(to: b[0:1024])
