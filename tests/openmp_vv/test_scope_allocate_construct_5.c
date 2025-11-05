#pragma omp scope private(arr) allocate(allocator(omp_low_lat_mem_alloc) : arr)
