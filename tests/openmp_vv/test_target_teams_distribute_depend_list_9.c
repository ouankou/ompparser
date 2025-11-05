#pragma omp target teams distribute nowait depend(out: e) map(alloc: b[0:1024], e[0:1024], g[0:1024])
