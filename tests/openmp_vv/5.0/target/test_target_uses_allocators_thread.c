#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel uses_allocators(omp_thread_mem_alloc) allocate(omp_thread_mem_alloc: x) private(x) map(from: device_result)
#pragma omp target map (from: _ompvv_isOffloadingOn)
