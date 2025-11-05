!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target defaultmap(none) map(tofrom: scalar, A, B, new_struct,ptr)
!$omp     end target
!$omp     target defaultmap(none) map(to: scalar, A, B, new_struct, ptr)
!$omp     end target
