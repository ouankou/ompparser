!$omp        target teams distribute map(to: a(1:1024)) reduction(.eqv.:result) map(tofrom: result)
