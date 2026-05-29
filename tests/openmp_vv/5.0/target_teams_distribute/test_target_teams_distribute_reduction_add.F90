!$omp     target teams distribute map(to: a(1:1024), b(1:1024)) reduction(+:dev_sum) map(tofrom: dev_sum)
