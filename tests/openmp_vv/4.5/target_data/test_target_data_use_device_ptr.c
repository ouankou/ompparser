#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(tofrom: array_device[0:1000])
#pragma omp target data map(map_dummy) use_device_ptr(array_device)
#pragma omp target is_device_ptr(array_device) map(tofrom: array_host[0:1000])
