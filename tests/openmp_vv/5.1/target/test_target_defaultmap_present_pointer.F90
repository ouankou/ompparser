!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target enter data map(to: ptr)
!$omp     target map(tofrom: errors) defaultmap(present:pointer)
!$omp     end target
!$omp     target exit data map(from: ptr)
