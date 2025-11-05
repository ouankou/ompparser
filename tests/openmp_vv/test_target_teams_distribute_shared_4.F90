!$omp     target teams distribute num_teams(10) shared(share) defaultmap(tofrom:scalar) map(to: a(1:1024))
