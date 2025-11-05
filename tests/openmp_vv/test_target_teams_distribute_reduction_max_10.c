#pragma omp target teams distribute reduction(max:result) map(to: a[0:1024], b[0:1024]) map(tofrom: result, num_teams[0:1024])
