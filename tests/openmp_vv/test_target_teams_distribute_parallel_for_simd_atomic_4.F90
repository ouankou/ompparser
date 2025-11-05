!$omp     target teams distribute parallel do simd map(tofrom: x) shared(x) num_teams(8) num_threads(8)
