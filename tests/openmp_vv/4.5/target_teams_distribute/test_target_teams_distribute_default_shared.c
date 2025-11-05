#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: a[0:1024]) map(tofrom: share, num_teams)
#pragma omp target teams distribute default(shared) defaultmap(tofrom:scalar) num_teams(8)
#pragma omp atomic
#pragma omp target data map(tofrom: a[0:1024]) map(tofrom: share)
#pragma omp target teams distribute default(shared) defaultmap(tofrom:scalar) num_teams(8)
