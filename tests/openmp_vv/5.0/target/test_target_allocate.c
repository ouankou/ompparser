#pragma omp target uses_allocators(omp_default_mem_alloc) allocate(omp_default_mem_alloc:x) firstprivate(x) map(from: device_result)
#pragma omp target map (from: _ompvv_isOffloadingOn)
