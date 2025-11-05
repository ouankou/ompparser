!$omp requires unified_shared_memory
!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp requires unified_shared_memory
!$omp     target is_device_ptr(anArrayDummy)
!$omp     end target
!$omp     target is_device_ptr(anArrayDummy)
!$omp     end target
