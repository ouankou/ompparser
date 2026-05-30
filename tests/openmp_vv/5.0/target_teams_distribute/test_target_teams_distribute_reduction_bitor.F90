!$omp        target teams distribute map(to: a(1:1024)) reduction(ior:result) map(tofrom: result)
