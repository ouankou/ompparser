#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: errors, A) uses_allocators(omp_default_mem_alloc)
#pragma omp allocate(x) allocator(omp_default_mem_alloc)
#pragma omp parallel for
#pragma omp target map (from: _ompvv_isOffloadingOn)
