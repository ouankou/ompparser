#pragma omp target teams distribute nowait depend(in:d) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024])
