#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: max_teams) if(offload)
#pragma omp target map(tofrom : num_teams) if(offload)
#pragma omp teams
#pragma omp target map (from: _ompvv_isOffloadingOn)
