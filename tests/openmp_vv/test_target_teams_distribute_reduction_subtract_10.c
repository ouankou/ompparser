#pragma omp target teams distribute reduction(-:total) map(to: a[0:1024], b[0:1024]) map(tofrom: total, num_teams[0:1024])
