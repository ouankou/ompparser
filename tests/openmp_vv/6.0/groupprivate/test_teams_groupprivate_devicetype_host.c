#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp groupprivate(group_sum) device_type(host)
#pragma omp teams num_teams(4) reduction(+: team_sum)
