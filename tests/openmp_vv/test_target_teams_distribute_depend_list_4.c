#pragma omp target data map(to: a[0:1024], b[0:1024]) map(alloc: c[0:1024], d[0:1024], e[0:1024]) map(from: f[0:1024], g[0:1024])
