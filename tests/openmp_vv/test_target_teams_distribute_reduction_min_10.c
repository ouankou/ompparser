#pragma omp target teams distribute reduction(min:result) map(to: a[0:1024], b[0:1024]) map(tofrom: result, num_teams[0:1024])
