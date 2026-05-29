#pragma omp target data map(alloc: h_array_h[0:1000])
#pragma omp target is_device_ptr(d_sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
