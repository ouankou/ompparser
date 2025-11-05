!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   requires dynamic_allocators
!$omp     target defaultmap(tofrom)
!$omp     allocate(x) allocator(x_alloc)
!$omp     parallel
!$omp     do simd simdlen(16) aligned(x: 64)
!$omp     end parallel
!$omp     end target
