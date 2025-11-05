#pragma omp target data map(to: a[0:1024]) map(tofrom: share, num_teams)
