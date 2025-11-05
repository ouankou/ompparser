#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute parallel for firstprivate(privatized, firstized, i) num_teams(8) num_threads(8)
#pragma omp target map (from: _ompvv_isOffloadingOn)
