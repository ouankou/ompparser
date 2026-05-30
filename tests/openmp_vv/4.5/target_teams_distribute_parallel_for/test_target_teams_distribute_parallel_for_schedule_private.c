#pragma omp target teams distribute parallel for firstprivate(firstprivatized) num_teams(8) num_threads(8) schedule(static,8)
#pragma omp target map (from: _ompvv_isOffloadingOn)
