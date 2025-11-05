#pragma omp target enter data map(to: a[0:1024]) map(alloc: b[0:1024])
