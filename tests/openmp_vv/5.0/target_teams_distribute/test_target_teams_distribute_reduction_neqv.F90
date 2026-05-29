!$omp        target teams distribute map(to: a(1:1024)) reduction(.neqv.: result) map(tofrom: result)
