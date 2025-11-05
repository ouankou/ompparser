#pragma omp teams distribute num_teams(8) default(none) shared(a, b, c, d, num_teams) private(privatized)
