#pragma omp target teams map(to: a[0:1024]) map(tofrom: num_actual_teams) reduction(+:total)
#pragma omp target map (from: _ompvv_isOffloadingOn)
