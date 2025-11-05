#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires unified_shared_memory
#pragma omp target is_device_ptr(aPtr)
#pragma omp target is_device_ptr(aPtr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
