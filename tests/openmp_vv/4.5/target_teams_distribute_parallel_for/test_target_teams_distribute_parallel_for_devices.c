#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: a[0:1024]) device(dev)
#pragma omp target teams distribute parallel for device(dev) map(tofrom: isHost)
#pragma omp target exit data map(from: a[0:1024]) device(dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
