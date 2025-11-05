!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(alloc: scalar, a, test_struct) map(to: scalar, a, test_struct) map(tofrom: errors)
!$omp     end target
