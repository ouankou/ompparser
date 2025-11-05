!$omp     target teams distribute nowait depend(out: c, d, e) map(alloc:c(1:1024), d(1:1024), e(1:1024))
