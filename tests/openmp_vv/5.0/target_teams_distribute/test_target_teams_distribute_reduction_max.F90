!$omp        target teams distribute map(to: a(1:1024)) reduction(max:result) map(tofrom: result)
