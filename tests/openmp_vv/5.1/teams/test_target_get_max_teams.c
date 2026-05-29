#pragma omp target map(tofrom: max_teams) if(offload)
#pragma omp target map(tofrom : num_teams) if(offload)
#pragma omp teams
#pragma omp target map (from: _ompvv_isOffloadingOn)
