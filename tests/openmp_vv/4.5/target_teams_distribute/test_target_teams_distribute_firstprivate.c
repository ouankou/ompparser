#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(from: d[0:1024]) map(to: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute firstprivate(privatized_array, privatized) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024]) num_teams(8)
#pragma omp target data map(from: d[0:1024]) map(to: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute firstprivate(privatized_array, privatized) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024]) num_teams(8)
