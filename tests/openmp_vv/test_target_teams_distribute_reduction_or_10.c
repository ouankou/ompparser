#pragma omp target teams distribute reduction(||:result) map(to: a[0:1024]) map(tofrom: result, num_teams[0:1024])
