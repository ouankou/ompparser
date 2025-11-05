!$omp     target teams distribute default(none) shared(a, b, c, d) private(x, y, privatized) num_teams(8)
