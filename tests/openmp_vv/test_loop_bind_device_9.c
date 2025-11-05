#pragma omp target teams num_teams(8) thread_limit(8) map(tofrom: result, y, z, num_teams)
