#pragma omp target teams distribute map(from: num_teams) map(tofrom: b[0:1024]) dist_schedule(static)
