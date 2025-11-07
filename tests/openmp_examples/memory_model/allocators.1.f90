!$omp    allocators allocate(allocator(xy_alloc): x, y)
!$omp    parallel
!$omp       do simd simdlen(16) aligned(x,y: 64)
!$omp       do simd simdlen(16) aligned(x,y: 64)
!$omp    end parallel
