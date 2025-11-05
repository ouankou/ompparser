#pragma omp target teams distribute nowait depend(out: random_data) map(alloc: b[0:1024], c[0:1024], d[0:1024])
