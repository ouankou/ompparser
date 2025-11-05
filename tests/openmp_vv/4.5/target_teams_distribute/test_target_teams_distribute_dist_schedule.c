#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute map(from: num_teams) map(tofrom: a[0:1024]) dist_schedule(static, 64)
#pragma omp target teams distribute map(from: num_teams) map(tofrom: b[0:1024]) dist_schedule(static)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
