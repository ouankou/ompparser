!$omp     target teams distribute nowait depend(inout: c) map(alloc: a(1:1024), b(1:1024), c(1:1024))
