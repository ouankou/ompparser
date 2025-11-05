#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: a[0:1024], b[0:1024]) map(alloc: c[0:1024], d[0:1024], e[0:1024]) map(from: f[0:1024], g[0:1024])
#pragma omp target teams distribute nowait depend(out: c) map(alloc: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute nowait depend(out: d) map(alloc: a[0:1024], b[0:1024], d[0:1024])
#pragma omp target teams distribute nowait depend(out: c, d, e) map(alloc: c[0:1024], d[0:1024], e[0:1024])
#pragma omp target teams distribute nowait depend(out: e) map(alloc: a[0:1024], e[0:1024], f[0:1024])
#pragma omp target teams distribute nowait depend(out: e) map(alloc: b[0:1024], e[0:1024], g[0:1024])
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
