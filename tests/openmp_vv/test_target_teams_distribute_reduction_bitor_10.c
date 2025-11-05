#pragma omp target teams distribute reduction(|:b) map(to: a[0:1024]) map(tofrom: b) map(from: num_teams[0:1024])
