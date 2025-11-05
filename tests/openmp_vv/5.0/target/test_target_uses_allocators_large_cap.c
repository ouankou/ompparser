#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target uses_allocators(omp_large_cap_mem_alloc) allocate(omp_large_cap_mem_alloc: x) firstprivate(x) map(from: device_result)
#pragma omp target map (from: _ompvv_isOffloadingOn)
