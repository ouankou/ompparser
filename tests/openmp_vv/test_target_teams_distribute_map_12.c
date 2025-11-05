#pragma omp target teams distribute map(alloc: a[0:1024], b[0:1024], c[0:1024])
