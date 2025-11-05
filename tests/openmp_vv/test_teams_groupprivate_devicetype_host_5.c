#pragma omp teams num_teams(4) reduction(+: team_sum)
