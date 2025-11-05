!$omp     target teams num_teams(8                     ) thread_limit(8)private(x) map(tofrom: outData, y, z, num_teams)
