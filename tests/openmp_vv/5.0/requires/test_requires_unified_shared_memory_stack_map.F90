!$omp requires unified_shared_memory
!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp requires unified_shared_memory
!$omp     target map(aPtr)
!$omp     end target
!$omp     target map(aPtr)
!$omp     end target
