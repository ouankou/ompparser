!$omp        target teams distribute map(to: a(1:1024)) reduction(ieor:result) map(tofrom: result)
