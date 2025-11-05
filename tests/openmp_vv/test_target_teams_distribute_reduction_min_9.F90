!$omp        target teams distribute map(to: a(1:1024)) reduction(min:result) map(tofrom: result)
