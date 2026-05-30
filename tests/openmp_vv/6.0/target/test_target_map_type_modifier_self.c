#pragma omp requires unified_shared_memory
#pragma omp target map(self : scalar_value) map(tofrom : device_address)
#pragma omp target map (from: _ompvv_isOffloadingOn)
