!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target uses_allocators(omp_large_cap_mem_alloc) allocate(omp_large_cap_mem_alloc: x) firstprivate(x) map(from: device_result)
!$omp     end target
