!$omp     target data map(to: a(1:1024), b(1:1024), c(1:1024)) map(tofrom: privatized(1:2))
