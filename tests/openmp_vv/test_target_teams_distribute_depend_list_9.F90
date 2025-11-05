!$omp     target teams distribute nowait depend(out: e) map(alloc: b(1:1024), e(1:1024), g(1:1024))
