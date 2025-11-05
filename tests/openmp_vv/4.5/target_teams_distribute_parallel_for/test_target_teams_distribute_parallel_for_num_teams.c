#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for map(tofrom: num_teams) num_teams(tested_num_teams[nt])
#pragma omp target map (from: _ompvv_isOffloadingOn)
