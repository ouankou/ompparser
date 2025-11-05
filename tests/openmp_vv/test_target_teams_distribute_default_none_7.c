#pragma omp target data map(from: num_teams) map(to: b[0:1024])
