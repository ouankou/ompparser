#pragma omp target teams default(firstprivate) map(tofrom:num_teams,errors) shared(num_teams,errors) num_teams(8)
