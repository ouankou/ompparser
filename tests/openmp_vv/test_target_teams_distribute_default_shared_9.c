#pragma omp target teams distribute default(shared) defaultmap(tofrom:scalar) num_teams(8)
