#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target teams distribute map(tofrom: default_num_teams, c[0:1024]) map(to: a[0:1024], b[0:1024])
#pragma omp target teams distribute num_teams(default_num_teams / 2) map(to: a[0:1024], b[0:1024]) map(from: c[0:1024], num_teams[0:1024])
