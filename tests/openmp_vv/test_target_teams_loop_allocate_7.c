#pragma omp target teams loop map(to: a[0:1024]) map(from: b[0:1024]) firstprivate(local) uses_allocators(omp_const_mem_alloc) allocate(omp_const_mem_alloc: local)
