!$omp     target teams distribute default(none) private(x) shared(share,a) defaultmap(tofrom:scalar) num_teams(8)
