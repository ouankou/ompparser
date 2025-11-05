#pragma omp teams distribute num_teams(8) default(none) shared(share, b, num_teams)
