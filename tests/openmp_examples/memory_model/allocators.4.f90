!$omp      declare target
!$omp    allocate(v1,v2) allocator(omp_high_bw_mem_alloc)
!$omp    allocate(v3,v4) allocator(omp_default_mem_alloc)
!$omp    target map(to: v3, v4) map(from:v3)
!$omp    end target
!$omp    task private(v5,v6) allocate(allocator(omp_low_lat_mem_alloc):v5,v6)
!$omp    end task
