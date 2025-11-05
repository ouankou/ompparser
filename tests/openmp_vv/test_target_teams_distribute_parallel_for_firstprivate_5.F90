!$omp          target teams distribute parallel do firstprivate(privatized, firstized, i) num_threads(8) num_teams(8)
