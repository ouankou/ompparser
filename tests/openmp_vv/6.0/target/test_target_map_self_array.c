#pragma omp requires unified_shared_memory
#pragma omp target map(self: arr) map(from: device_pointer)
#pragma omp target map (from: _ompvv_isOffloadingOn)
