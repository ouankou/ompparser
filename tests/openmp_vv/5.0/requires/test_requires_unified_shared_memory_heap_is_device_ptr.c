#pragma omp requires unified_shared_memory
#pragma omp target is_device_ptr(anArray)
#pragma omp target is_device_ptr(anArray)
#pragma omp target map (from: _ompvv_isOffloadingOn)
