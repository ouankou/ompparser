!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(tofrom: a, new_struct)
!$omp     target map(tofrom: errors) defaultmap(present:aggregate)
!$omp     end target
!$omp     end target data
