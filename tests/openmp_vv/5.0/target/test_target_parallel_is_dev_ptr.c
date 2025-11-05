#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: Hst_A[0:1024]) device(gpu)
#pragma omp target parallel for is_device_ptr(Dev_B) device(gpu)
#pragma omp target map (from: _ompvv_isOffloadingOn)
