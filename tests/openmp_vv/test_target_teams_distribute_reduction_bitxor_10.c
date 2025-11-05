#pragma omp target teams distribute reduction(^:b) map(to: a[0:1024]) map(tofrom: b, num_teams[0:1024])
