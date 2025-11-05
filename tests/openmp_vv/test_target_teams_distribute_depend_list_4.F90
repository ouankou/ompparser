!$omp     target data map(to: a(1:1024), b(1:1024)) map(alloc: c(1:1024), d(1:1024), e(1:1024)) map(from: f(1:1024), g(1:1024))
