#pragma omp target teams map(to: a[0:1024]) map(tofrom: num_actual_teams) reduction(+:total)
