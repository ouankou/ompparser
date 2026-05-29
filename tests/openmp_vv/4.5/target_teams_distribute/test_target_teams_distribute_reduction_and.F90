!$omp        target teams distribute map(to: a(1:1024)) reduction(.and.:result) defaultmap(tofrom: scalar)
