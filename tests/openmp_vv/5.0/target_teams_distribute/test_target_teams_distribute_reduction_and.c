#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute reduction(&&:result) map(to: a[0:1024]) map(tofrom: result, num_teams[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
