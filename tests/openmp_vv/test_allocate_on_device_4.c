#pragma omp target map(tofrom: errors, A) uses_allocators(omp_default_mem_alloc)
