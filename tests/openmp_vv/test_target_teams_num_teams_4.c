#pragma omp target teams map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) map(tofrom: num_actual_teams) num_teams(32)
