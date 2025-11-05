#pragma omp target teams distribute parallel for simd map(tofrom: x) shared(x) num_teams(8) num_threads(8)
