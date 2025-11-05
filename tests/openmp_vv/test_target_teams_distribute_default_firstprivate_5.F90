!$omp     target teams distribute default(firstprivate) shared(a, b, c,d, num_teams) num_teams(8)
