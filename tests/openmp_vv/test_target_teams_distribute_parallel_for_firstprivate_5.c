#pragma omp target teams distribute parallel for firstprivate(privatized, firstized, i) num_teams(8) num_threads(8)
