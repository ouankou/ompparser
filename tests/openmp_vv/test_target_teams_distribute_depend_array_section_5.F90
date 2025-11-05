!$omp     target teams distribute nowait depend(out: c(1:1024)) map(alloc: a(1:1024), b(1:1024), c(1:1024))
