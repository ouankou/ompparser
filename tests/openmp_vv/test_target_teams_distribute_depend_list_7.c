#pragma omp target teams distribute nowait depend(out: c, d, e) map(alloc: c[0:1024], d[0:1024], e[0:1024])
