#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: device_data[0:1024])
#pragma omp target data use_device_addr(device_data)
#pragma omp target teams map(from: host_data[0:1024]) is_device_ptr(dev_ptr)
