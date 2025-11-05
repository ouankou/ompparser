#pragma omp target teams distribute num_teams(8) shared(share, num_teams) map(to: a[0:1024]) defaultmap(tofrom:scalar)
