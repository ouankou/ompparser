#pragma omp target data map(to: a[0:1024], b[0:1024]) map(alloc: c[0:1024], random_data[0:1]) map(from: d[0:1024])
