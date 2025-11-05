#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(from: d[0:1024], num_teams) map(to: a[0:1024], b[0:1024], c[0:1024])
#pragma omp target teams distribute default(none) shared(a, b, c, d, num_teams) private(x, privatized) num_teams(8)
#pragma omp target data map(from: num_teams) map(to: b[0:1024])
#pragma omp target teams distribute default(none) private(x) shared(share, b, num_teams) defaultmap(tofrom:scalar) num_teams(8)
#pragma omp atomic update
