!$omp     target teams distribute nowait depend(out: e) map(alloc: a(1:1024), e(1:1024), f(1:1024))
