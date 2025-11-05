!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams distribute uses_allocators(omp_pteam_mem_alloc) allocate(omp_pteam_mem_alloc: x) private(x) map(from: device_result)
