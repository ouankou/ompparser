#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant(add_dev) match(construct={dispatch})
#pragma omp target parallel for is_device_ptr(arr)
#pragma omp target is_device_ptr(arr)
#pragma omp target is_device_ptr(arr)
#pragma omp parallel for
#pragma omp dispatch is_device_ptr(arr)
#pragma omp target map(tofrom: errors, called_add) is_device_ptr(arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
