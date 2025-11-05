#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: device_data)
#pragma omp target data use_device_addr(device_data)
#pragma omp target map(to:device_data) map(tofrom: errors) map(from: host_data) is_device_ptr(dev_ptr)
#pragma omp target map(from: host_data) is_device_ptr(dev_ptr)
