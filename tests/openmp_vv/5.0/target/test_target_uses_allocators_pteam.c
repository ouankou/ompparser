#pragma omp target parallel for map(from: device_result) uses_allocators(omp_pteam_mem_alloc) allocate(omp_pteam_mem_alloc: pteam_result) private(pteam_result)
#pragma omp target map (from: _ompvv_isOffloadingOn)
