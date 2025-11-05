#pragma omp target teams distribute map(tofrom: a[0:1024], num_teams[0:1024]) map(to: b[0:1024])
