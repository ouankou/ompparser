!$omp     target teams distribute nowait depend(in:d) map(alloc: a(1:1024), b(1:1024), c(1:1024), d(1:1024))
