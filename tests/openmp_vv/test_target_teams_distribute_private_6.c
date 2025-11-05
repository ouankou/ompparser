#pragma omp target teams distribute private(privatized) map(alloc: a[0:1024], b[0:1024], c[0:1024], d[0:1024]) map(tofrom: num_teams) num_teams(8)
