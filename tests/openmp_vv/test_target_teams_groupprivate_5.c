#pragma omp target teams num_teams(8) map(tofrom: team_sum) reduction(+: team_sum)
