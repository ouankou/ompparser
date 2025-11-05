#pragma omp target teams distribute parallel for map(from:num_teams, num_threads) num_teams(8) num_threads(8)
