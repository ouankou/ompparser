#pragma omp groupprivate(group_sum) device_type(host)
#pragma omp teams num_teams(4) reduction(+: team_sum)
