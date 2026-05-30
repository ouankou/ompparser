#pragma omp requires unified_shared_memory
#pragma omp target is_device_ptr(ptr) device(dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
