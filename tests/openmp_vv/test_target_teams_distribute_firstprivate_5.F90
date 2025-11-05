!$omp     target teams distribute firstprivate(privatized, privatized_array) map(alloc: a(1:1024), b(1:1024), c(1:1024), d(1:1024)) num_teams(8)
