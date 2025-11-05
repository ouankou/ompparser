#pragma omp target teams distribute nowait depend(in: c) map(alloc: a[0:1024], c[0:1024], d[0:1024])
