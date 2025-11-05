!$omp     target teams distribute map(tofrom: default_num_teams, c(1:1024)) map(to: a(1:1024), b(1:1024))
