#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(to: h_array_h[0:1000]) map(to: h_array_s[0:1000])
#pragma omp target is_device_ptr(d_array, d_array2)
#pragma omp target map (from: _ompvv_isOffloadingOn)
