#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams map(to: a[0:1024], b[0:1024]) map(from: c[0:1024]) map(tofrom: num_actual_teams) num_teams(32)
#pragma omp target map (from: _ompvv_isOffloadingOn)
