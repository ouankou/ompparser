#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target teams distribute num_teams(8) shared(share, num_teams) map(to: a[0:1024]) defaultmap(tofrom:scalar)
#pragma omp atomic write
#pragma omp atomic
#pragma omp target data map(tofrom: a[0:1024]) map(tofrom: share)
#pragma omp target teams distribute num_teams(8) shared(share)
