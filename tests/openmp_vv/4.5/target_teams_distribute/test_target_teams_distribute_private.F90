!$omp           target teams distribute private(privatized) map(to: a(1:1024), b(1:1024), c(1:1024)) map(from: d(1:1024)) map(tofrom: num_teams) num_teams(8)
