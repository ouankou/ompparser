!$omp     target data map(from: d(1:1024), num_teams(1:1024)) map(to: a(1:1024), b(1:1024), c(1:1024))
