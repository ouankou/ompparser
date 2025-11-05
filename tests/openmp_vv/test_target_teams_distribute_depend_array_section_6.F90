!$omp     target teams distribute nowait depend(out: c(1:1024)) map(alloc: b(1:1024), c(1:1024), d(1:1024))
