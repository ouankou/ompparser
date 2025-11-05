!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(tofrom: scalar, a, member)
!$omp     target map(present, tofrom: scalar, a, member)
!$omp     end target
!$omp     end target data
