#pragma omp target teams distribute nowait depend(out: c[0:1024/2]) map(alloc: a[0:1024], b[0:1024], d[0:1024])
