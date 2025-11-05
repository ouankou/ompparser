#pragma omp target teams distribute num_teams(default_num_teams / 2) map(to: a[0:1024], b[0:1024]) map(from: c[0:1024], num_teams[0:1024])
