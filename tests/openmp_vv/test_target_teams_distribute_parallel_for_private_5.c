#pragma omp target teams distribute parallel for private(privatized, i) num_threads(8) num_teams(8)
