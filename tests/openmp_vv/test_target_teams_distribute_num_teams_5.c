#pragma omp target teams distribute map(tofrom: default_num_teams, c[0:1024]) map(to: a[0:1024], b[0:1024])
