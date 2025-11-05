#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: array[:])
#pragma omp target data use_device_addr(array[:])
#pragma omp target has_device_addr(array[:8])
#pragma omp target map (from: _ompvv_isOffloadingOn)
