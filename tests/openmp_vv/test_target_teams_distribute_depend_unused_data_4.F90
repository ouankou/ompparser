!$omp     target data map(to: a(1:1024), b(1:1024)) map(alloc: c(1:1024), random_data(1:1)) map(from: d(1:1024))
