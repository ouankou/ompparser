#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for num_teams(2) thread_limit(4) map(tofrom: num_teams, num_threads) dist_schedule(static, 4)
#pragma omp target map (from: _ompvv_isOffloadingOn)
