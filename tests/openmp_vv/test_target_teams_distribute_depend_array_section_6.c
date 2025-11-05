#pragma omp target teams distribute nowait depend(out: c[0:1024]) map(alloc: b[0:1024], c[0:1024], d[0:1024])
