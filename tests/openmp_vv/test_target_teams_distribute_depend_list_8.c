#pragma omp target teams distribute nowait depend(out: e) map(alloc: a[0:1024], e[0:1024], f[0:1024])
