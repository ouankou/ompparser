#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant(target_func) match(device={kind(nohost)})
#pragma omp groupprivate(group_sum) device_type(nohost)
#pragma omp target teams num_teams(8) map(tofrom: team_sum, errors) reduction(+: team_sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
