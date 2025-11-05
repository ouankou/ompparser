#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: a[0:1024], b[0:1024]) map(alloc: c[0:1024], random_data[0:1]) map(from: d[0:1024])
#pragma omp target teams distribute nowait depend(out: random_data) map(alloc: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute nowait depend(out: random_data) map(alloc: b[0:1024], c[0:1024], d[0:1024])
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
