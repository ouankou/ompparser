#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: a[0:1024], b[0:1024]) map(tofrom: c[0:1024], d[0:1024])
#pragma omp target teams distribute nowait depend(in:d) map(alloc: a[0:1024], b[0:1024], d[0:1024])
#pragma omp atomic
#pragma omp target teams distribute nowait depend(in:d) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024])
#pragma omp atomic
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
