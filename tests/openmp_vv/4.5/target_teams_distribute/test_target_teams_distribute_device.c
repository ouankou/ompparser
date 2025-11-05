#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data map(to: a[dev][0:1024], b[0:1024], num_teams[dev:1]) device(dev)
#pragma omp target teams distribute map(alloc: a[dev][0:1024], b[0:1024], num_teams[dev:1]) device(dev)
#pragma omp target exit data map(from: a[dev][0:1024], num_teams[dev:1]) map(delete: b[0:1024]) device(dev)
