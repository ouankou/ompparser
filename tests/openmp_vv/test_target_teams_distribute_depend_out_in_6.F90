!$omp     target teams distribute nowait depend(in: c) map(alloc: b(1:1024), c(1:1024), d(1:1024))
