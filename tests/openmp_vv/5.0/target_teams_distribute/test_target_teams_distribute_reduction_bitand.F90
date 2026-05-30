!$omp        target teams distribute map(to: a(1:1024)) reduction(iand:result) map(tofrom: result)
