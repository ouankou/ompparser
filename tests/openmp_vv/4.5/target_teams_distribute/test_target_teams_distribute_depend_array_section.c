#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: a[0:1024], b[0:1024]) map(alloc: c[0:1024]) map(from: d[0:1024])
#pragma omp target teams distribute nowait depend(out: c[0:1024]) map(alloc: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute nowait depend(out: c[0:1024]) map(alloc: b[0:1024], c[0:1024], d[0:1024])
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
