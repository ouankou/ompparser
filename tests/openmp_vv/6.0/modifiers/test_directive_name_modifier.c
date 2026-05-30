#pragma omp target teams distribute firstprivate(target : shared_value) num_teams(8) map(from : final_shared)
#pragma omp atomic update
#pragma omp atomic write
#pragma omp target teams distribute firstprivate(teams : shared_value) num_teams(8) map(from : shared_val_arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
