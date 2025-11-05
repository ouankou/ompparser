#pragma omp target teams distribute default(none) private(x) shared(share, b, num_teams) defaultmap(tofrom:scalar) num_teams(8)
