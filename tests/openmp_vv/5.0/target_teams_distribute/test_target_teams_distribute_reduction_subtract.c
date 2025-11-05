#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute reduction(-:total) map(to: a[0:1024], b[0:1024]) map(tofrom: total, num_teams[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
