#pragma omp target teams distribute default(none) shared(a, b, c, d, num_teams) private(x, privatized) num_teams(8)
